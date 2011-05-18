/* base class for all vips operations
 */

/*

    Copyright (C) 1991-2005 The National Gallery

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA

 */

/*

    These files are distributed with VIPS - http://www.vips.ecs.soton.ac.uk

 */

/*
#define VIPS_DEBUG
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif /*HAVE_CONFIG_H*/
#include <vips/intl.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include <vips/vips.h>
#include <vips/debug.h>

#include <gobject/gvaluecollector.h>

#ifdef WITH_DMALLOC
#include <dmalloc.h>
#endif /*WITH_DMALLOC*/

/* Abstract base class for operations.
 */

G_DEFINE_ABSTRACT_TYPE( VipsOperation, vips_operation, VIPS_TYPE_OBJECT );

/* What to show about the argument.
 */
typedef struct {
	char *message;		/* header message on first print */
	gboolean required;	/* show required args or optional */
	gboolean oftype;	/* "is of type" message */
	int n;			/* Arg number */
} VipsOperationPrint;

static void *
vips_operation_print_arg( VipsObject *object, GParamSpec *pspec,
	VipsArgumentClass *argument_class,
	VipsArgumentInstance *argument_instance,
	VipsBuf *buf, VipsOperationPrint *print )
{
	/* Only show unassigned args ... assigned args are internal.
	 */

	if( print->required == 
		((argument_class->flags & VIPS_ARGUMENT_REQUIRED) != 0) &&
		!argument_instance->assigned ) {
		if( print->message && print->n == 0 ) 
			vips_buf_appendf( buf, "%s\n", print->message );

		if( print->oftype ) 
			vips_buf_appendf( buf, "   %s :: %s\n",
				g_param_spec_get_name( pspec ), 
				g_type_name( 
					G_PARAM_SPEC_VALUE_TYPE( pspec ) ) );
		else {
			if( print->n > 0 )
				vips_buf_appends( buf, ", " );
			vips_buf_appends( buf, g_param_spec_get_name( pspec ) );
		}

		print->n += 1;
	}

	return( NULL );
}

#ifdef VIPS_DEBUG
static void *
vips_operation_call_argument( VipsObject *object, GParamSpec *pspec,
	VipsArgumentClass *argument_class,
	VipsArgumentInstance *argument_instance )
{
	VipsArgument *argument = (VipsArgument *) argument_class;

	printf( "   %s: offset = %d ", 
		g_param_spec_get_name( argument->pspec ),
		argument_class->offset );
	if( argument_class->flags & VIPS_ARGUMENT_REQUIRED )
		printf ("required " );
	if( argument_class->flags & VIPS_ARGUMENT_CONSTRUCT )
		printf ("construct " );
	if( argument_class->flags & VIPS_ARGUMENT_SET_ONCE )
		printf ("set-once " );
	if( argument_instance->assigned )
		printf ("assigned " );
	printf( "\n" );

	return( NULL );
}
#endif /*VIPS_DEBUG*/

static void
vips_operation_print( VipsObject *object, VipsBuf *buf )
{
	VipsOperation *operation = VIPS_OPERATION( object );
	VipsObjectClass *object_class = VIPS_OBJECT_GET_CLASS( object );
	VipsOperationPrint print;

#ifdef VIPS_DEBUG
	printf( "%s args:\n", object_class->nickname );
	vips_argument_map( VIPS_OBJECT( operation ),
		(VipsArgumentMapFn) vips_operation_call_argument, NULL, NULL );
#endif /*VIPS_DEBUG*/

	/* First pass through args: show the required names.
	 */
	vips_buf_appendf( buf, "%s (", object_class->nickname );
	print.message = NULL;
	print.required = TRUE;
	print.oftype = FALSE;
	print.n = 0;
	vips_argument_map( VIPS_OBJECT( operation ),
		(VipsArgumentMapFn) vips_operation_print_arg, buf, &print );
	vips_buf_appends( buf, ")\n" );

	/* Show required types.
	 */
	print.message = "where:";
	print.required = TRUE;
	print.oftype = TRUE;
	print.n = 0;
	vips_argument_map( VIPS_OBJECT( operation ),
		(VipsArgumentMapFn) vips_operation_print_arg, buf, &print );

	/* Show optional args.
	 */
	print.message = "optional arguments:";
	print.required = FALSE;
	print.oftype = TRUE;
	print.n = 0;
	vips_argument_map( VIPS_OBJECT( operation ),
		(VipsArgumentMapFn) vips_operation_print_arg, buf, &print );
}

static int
vips_operation_build( VipsObject *object )
{
	if( VIPS_OBJECT_CLASS( vips_operation_parent_class )->build( object ) )
		return( -1 );

	return( 0 );
}

static void
vips_operation_class_init( VipsOperationClass *class )
{
	VipsObjectClass *vobject_class = VIPS_OBJECT_CLASS( class );

	vobject_class->print = vips_operation_print;
	vobject_class->build = vips_operation_build;
}

static void
vips_operation_init( VipsOperation *operation )
{
	/* Init our instance fields.
	 */
}

typedef enum {
	OPTIONAL = 0x1,
	REQUIRED = 0x2
} ArgFlags;

static int
vips_operation_set_valist (VipsOperation * operation,
				    ArgFlags flags, va_list ap)
{
  VipsObject *object = VIPS_OBJECT (operation);
  VipsObjectClass *class = VIPS_OBJECT_GET_CLASS (object);
  GSList *p;

  if (flags & REQUIRED)
    {
      /* Extract required arguments. Can't use vips_argument_map here 
       * :-( because passing va_list by reference is not portable. 
       * So we have to copy-paste the vips_argument_map() loop. 
       * Keep in sync with that.
       */

      for (p = class->argument_table_traverse; p; p = p->next)
	{
	  VipsArgumentClass *argument_class = (VipsArgumentClass *) p->data;
	  VipsArgument *argument = (VipsArgument *) argument_class;
	  GParamSpec *pspec = argument->pspec;
	  VipsArgumentInstance *argument_instance =
	    vips__argument_get_instance (argument_class, object);

	  /* We have many props on the arg table ... filter out the ones
	   * for this class.
	   */
	  if (g_object_class_find_property (G_OBJECT_CLASS (class),
				  g_param_spec_get_name( pspec )) == pspec)
	    {

	      /* End of stuff copy-pasted from vips_argument_map().
	       */
	      if (argument_class->flags & VIPS_ARGUMENT_REQUIRED &&
		  !argument_instance->assigned)
		{
		  GValue value = { 0 };
		  char *msg = NULL;

		  g_value_init (&value, G_PARAM_SPEC_VALUE_TYPE (pspec));
		  G_VALUE_COLLECT (&value, ap, 0, &msg);
		  if (msg)
		    {
		      vips_error (class->description, "%s", _(msg));
		      g_value_unset (&value);
		      g_free (msg);
		      return (-1);
		    }

#ifdef VIPS_DEBUG
		  {
		    char *str;

		    str = g_strdup_value_contents (&value);
		    VIPS_DEBUG_MSG ("\t%s = %s\n", 
				    g_param_spec_get_name( pspec ), str);
		    g_free (str);
		  }
#endif /*VIPS_DEBUG */

		  g_object_set_property (G_OBJECT (operation),
				  g_param_spec_get_name( pspec ), &value);
		  g_value_unset (&value);
		}
	    }
	}
    }

  if (flags & OPTIONAL)
    {
      char *first_property_name;

      first_property_name = va_arg (ap, char *);
      g_object_set_valist (G_OBJECT (operation), first_property_name, ap);
    }

  return (0);
}

VipsOperation *
vips_operation_new( const char *name )
{
	GType type;

	if( !(type = vips_type_find( "VipsOperation", name )) )
		return( NULL );

	return( VIPS_OPERATION( g_object_new( type, NULL ) ) );
}

int
vips_call( const char *operation_name, ... )
{
	VipsOperation *operation;
	int result;
	va_list ap;


	VIPS_DEBUG_MSG( "vips_call: starting for %s ...\n", operation_name );

	if( !(operation = vips_operation_new( operation_name ) ) )
		return( -1 );

#ifdef VIPS_DEBUG
	VIPS_DEBUG_MSG( "where:\n" );
	vips_object_print( VIPS_OBJECT( operation ) );
#endif /*VIPS_DEBUG*/

	va_start( ap, operation_name );
	result = vips_operation_set_valist( operation, 
			REQUIRED | OPTIONAL, ap ) ||
		vips_object_build( VIPS_OBJECT( operation ) );
	va_end( ap );

	/* The operation we have built should now have been reffed by one of 
	 * its arguments or have finished its work. Either way, we can unref.
	 */
	g_object_unref( operation );

	return( result );
}

static int
vips_call_split_valist( const char *operation_name, 
	va_list required, va_list optional ) 
{
	VipsOperation *operation;
	int result;

	VIPS_DEBUG_MSG( "vips_call: starting for %s ...\n", operation_name );

	if( !(operation = vips_operation_new( operation_name ) ) )
		return( -1 );

#ifdef VIPS_DEBUG
	VIPS_DEBUG_MSG( "where:\n" );
	vips_object_print( VIPS_OBJECT( operation ) );
#endif /*VIPS_DEBUG*/

	result = vips_operation_set_valist( operation, REQUIRED, required ) ||
		vips_operation_set_valist( operation, OPTIONAL, optional ) ||
		vips_object_build( VIPS_OBJECT( operation ) );

	/* The operation we have built should now have been reffed by one of 
	 * its arguments or have finished its work. Either way, we can unref.
	 */
	g_object_unref( operation );

	return( result );
}

int
vips_call_split( const char *operation_name, va_list optional, ... ) 
{
	int result;

	va_list required;

	va_start( required, optional );
	result = vips_call_split_valist( operation_name, required, optional );
	va_end( required );

	return( result );
}

static int
vips_call_argv_set_required( VipsOperation *operation, const char *value )
{
	GParamSpec *pspec;

	/* Search for the first unset required argument.
	 */
	if( !(pspec = vips_argument_map( VIPS_OBJECT( operation ),
		vips_argument_is_required, NULL, NULL )) ) {
		vips_error( "VipsOperation",
			_( "no unset required arguments for %s" ), value );
		return( -1 );
	}

	if( vips_object_set_argument_from_string( VIPS_OBJECT( operation ), 
		g_param_spec_get_name( pspec ), value )  )
		return( -1 );

	return( 0 );
}

static void *
vips_call_char_option( VipsObject *object,
	GParamSpec *pspec,
	VipsArgumentClass *argument_class,
	VipsArgumentInstance *argument_instance,
	void *a, void *b )
{
	const char *name = (const char *) a;
	const char *value = (const char *) b;

	if( !(argument_class->flags & VIPS_ARGUMENT_REQUIRED) &&
		(argument_class->flags & VIPS_ARGUMENT_CONSTRUCT) &&
		!argument_instance->assigned &&
		g_param_spec_get_name( pspec )[0] == name[0] ) 
		if( vips_object_set_argument_from_string( object, 
			g_param_spec_get_name( pspec ), value ) )
			return( object );

	return( NULL );
}

static void *
vips_call_name_option( VipsObject *object,
	GParamSpec *pspec,
	VipsArgumentClass *argument_class,
	VipsArgumentInstance *argument_instance,
	void *a, void *b )
{
	const char *name = (const char *) a;
	const char *value = (const char *) b;

	if( !(argument_class->flags & VIPS_ARGUMENT_REQUIRED) &&
		(argument_class->flags & VIPS_ARGUMENT_CONSTRUCT) &&
		!argument_instance->assigned &&
		strcmp( g_param_spec_get_name( pspec ), name  ) == 0 ) 
		if( vips_object_set_argument_from_string( object, 
			g_param_spec_get_name( pspec ), value ) )
			return( object );

	return( NULL );
}

static gboolean
vips_call_options_set( const gchar *option_name, const gchar *value, 
	gpointer data, GError **error )
{
	VipsOperation *operation = (VipsOperation *) data;
	const char *name;

	VIPS_DEBUG_MSG( "vips_call_options_set: %s = %s\n", 
		option_name, value );

	/* Remove any leading "--" from the option name.
	 */
	for( name = option_name; *name == '-'; name++ )
		;

	/* If this is a single-character name, find the first unset pspec with
	 * that initial. Otherwise, search for a spec of that nmae.
	 */
	if( strlen( name ) == 1 ) {
		if( vips_argument_map( VIPS_OBJECT( operation ),
			vips_call_char_option, 
			(void *) name, (void *) value ) ) {
			vips_error_g( error );
			return( FALSE );
		}
	}
	else {
		if( vips_argument_map( VIPS_OBJECT( operation ),
			vips_call_name_option, 
			(void *) name, (void *) value ) ) {
			vips_error_g( error );
			return( FALSE );
		}
	}

	return( TRUE );
}

static void *
vips_call_options_add( VipsObject *object,
	GParamSpec *pspec,
	VipsArgumentClass *argument_class,
	VipsArgumentInstance *argument_instance,
	void *a, void *b )
{
	GOptionGroup *group = (GOptionGroup *) a;

	if( !(argument_class->flags & VIPS_ARGUMENT_REQUIRED) &&
		(argument_class->flags & VIPS_ARGUMENT_CONSTRUCT) &&
		!argument_instance->assigned ) {
		GOptionEntry entry[2];

		entry[0].long_name = g_param_spec_get_name( pspec );
		entry[0].short_name = g_param_spec_get_name( pspec )[0];
		if( G_IS_PARAM_SPEC_BOOLEAN( pspec ) ) 
			entry[0].flags = G_OPTION_FLAG_NO_ARG;
		entry[0].arg = G_OPTION_ARG_CALLBACK;
		entry[0].arg_data = (gpointer) vips_call_options_set;
		entry[0].description = g_param_spec_get_blurb( pspec );
		if( G_IS_PARAM_SPEC_BOOLEAN( pspec ) ) 
			entry[0].arg_description = NULL;
		else
			entry[0].arg_description = 
				g_type_name( G_PARAM_SPEC_VALUE_TYPE( pspec ) );

		entry[1].long_name = NULL;

		VIPS_DEBUG_MSG( "vips_call_options_add: adding %s\n",
			g_param_spec_get_name( pspec ) );

		g_option_group_add_entries( group, &entry[0] );
	}

	return( NULL );
}

void
vips_call_options( GOptionGroup *group, VipsOperation *operation )
{
	(void) vips_argument_map( VIPS_OBJECT( operation ),
		vips_call_options_add, group, NULL );
}

/* Our main command-line entry point. Optional args should have been set by
 * the GOption parser already, see above.
 *
 * We don't create the operation, so we must not unref it. The caller must
 * unref on error too.
 */
int
vips_call_argv( VipsOperation *operation, int argc, char **argv )
{
	int i;

	g_assert( argc >= 0 );

#ifdef VIPS_DEBUG
	printf( "vips_call_argv: " );
	vips_object_print_name( VIPS_OBJECT( operation ) );
	printf( "\n" );
	for( i = 0; i < argc; i++ )
		printf( "%d) %s\n", i, argv[i] );
#endif /*VIPS_DEBUG*/


	/* Now set required args from the rest of the command-line. 
	 */
	for( i = 0; i < argc; i++ )
		if( vips_call_argv_set_required( operation, argv[i] ) ) 
			return( -1 );

	if( vips_object_build( VIPS_OBJECT( operation ) ) ) 
		return( -1 );

	return( 0 );
}
