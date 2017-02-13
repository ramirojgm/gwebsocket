/*
	Copyright (C) 2017 Ramiro Jose Garcia Moraga

	This file is free software: you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation, either version 3 of the License, or
	(at your option) any later version.

	This file is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with the this software.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef HTTP_PACKAGE_
#define HTTP_PACKAGE_

#include <glib.h>
#include <gio/gio.h>

#define HTTP_TYPE_PACKAGE	(http_package_get_type())

G_DECLARE_DERIVABLE_TYPE(HttpPackage,http_package,HTTP,PACKAGE,GObject)

struct _HttpPackageClass
{
	/* parent class */
	GObjectClass parent_class;
	/* methods */
	gboolean (*read_from_stream)(HttpPackage * package,GDataInputStream * stream,gsize * length,GCancellable * cancellable,GError ** error);
	gboolean (*write_to_stream)(HttpPackage * package,GOutputStream * stream,gsize * length,GCancellable * cancellable,GError ** error);
	/* padding */
	gpointer padding[12];
};

G_BEGIN_DECLS

GDataInputStream
*		http_data_input_stream(GInputStream * stream,gsize * length,GCancellable * cancellable,GError ** error);

gboolean	http_package_read_from_stream(HttpPackage * package,GDataInputStream * stream,gsize * length,GCancellable * cancellable,GError ** error);
gboolean	http_package_write_to_stream(HttpPackage * package,GOutputStream * stream,gsize * length,GCancellable * cancellable,GError ** error);

void		http_package_reset(HttpPackage * package);

gboolean	http_package_is_set(HttpPackage * package,const gchar * name);

void		http_package_unset(HttpPackage * package,const gchar * name);

gint		http_package_get_int(HttpPackage * package,const gchar * name);
void		http_package_set_int(HttpPackage * package,const gchar * name,gint value);

gint64		http_package_get_int64(HttpPackage * package,const gchar * name);
void		http_package_set_int64(HttpPackage * package,const gchar * name,gint64 value);

gfloat		http_package_get_float(HttpPackage * package,const gchar * name);
void		http_package_set_float(HttpPackage * package,const gchar * name,gfloat value);

gdouble		http_package_get_double(HttpPackage * package,const gchar * name);
void		http_package_set_double(HttpPackage * package,const gchar * name,gdouble value);

const gchar *	http_package_get_string(HttpPackage * package,const gchar * name,gsize * length);
void		http_package_set_string(HttpPackage * package,const gchar * name,const gchar * value,gsize length);

gchar *		http_string_encode(const gchar * str1,gsize length);
gchar *		http_string_decode(const gchar * str1,gsize length);

G_END_DECLS

#endif
