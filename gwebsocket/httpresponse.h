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

#ifndef HTTP_RESPONSE_
#define HTTP_RESPONSE_

#include "httppackage.h"

#define HTTP_TYPE_RESPONSE	(http_response_get_type())

G_DECLARE_DERIVABLE_TYPE(HttpResponse,http_response,HTTP,RESPONSE,HttpPackage)

typedef enum _HttpResponseCode HttpResponseCode;

enum _HttpResponseCode
{
	HTTP_RESPONSE_OK = 0,
	HTTP_RESPONSE_BAD_REQUEST = 1,
	HTTP_RESPONSE_NOT_FOUND = 2,
	HTTP_RESPONSE_INTERNAL_ERROR = 3,
	HTTP_RESPONSE_SWITCHING_PROTOCOLS = 4,
	HTTP_RESPONSE_FORBIDDEN = 5,
	HTTP_RESPONSE_INVALID = 6
};

struct _HttpResponseClass
{
	/* parent class */
	HttpPackageClass parent_class;
	/* methods */
	gpointer padding[12];
};

G_BEGIN_DECLS

HttpResponse *		http_response_new(HttpResponseCode code,gdouble version);

HttpResponseCode	http_response_get_code(HttpResponse * response);
void			http_response_set_code(HttpResponse * response,HttpResponseCode code);

gdouble			http_response_get_version(HttpResponse * response);
void			http_response_set_version(HttpResponse * response,gdouble version);

G_END_DECLS

#endif
