#ifndef _HTTP_REQUEST_
#define _HTTP_REQUEST_

#include "httppackage.h"

#define HTTP_TYPE_REQUEST	(http_request_get_type())

G_DECLARE_DERIVABLE_TYPE(HttpRequest,http_request,HTTP,REQUEST,HttpPackage)

typedef enum _HttpRequestMethod HttpRequestMethod;

enum _HttpRequestMethod
{
	HTTP_REQUEST_METHOD_GET = 0,
	HTTP_REQUEST_METHOD_POST = 1,
	HTTP_REQUEST_METHOD_HEAD = 2,
	HTTP_REQUEST_METHOD_PUT = 3,
	HTTP_REQUEST_METHOD_DELETE = 4,
	HTTP_REQUEST_METHOD_TRACE = 5,
	HTTP_REQUEST_METHOD_OPTIONS = 6,
	HTTP_REQUEST_METHOD_CONNECT = 7,
	HTTP_REQUEST_METHOD_PATCH = 8,
	HTTP_REQUEST_METHOD_INVALID = 9
};

struct _HttpRequestClass
{
	/* parent class */
	HttpPackageClass parent_class;
	/* methods */
	gpointer padding[12];
};

G_BEGIN_DECLS

HttpRequest *		http_request_new(HttpRequestMethod method,const gchar * query,gdouble version);

HttpRequestMethod	http_request_get_method(HttpRequest * request);
void			http_request_set_method(HttpRequest * request,HttpRequestMethod method);

const gchar *		http_request_get_query(HttpRequest * request);
void			http_request_set_query(HttpRequest * request,const gchar * query);

gdouble			http_request_get_version(HttpRequest * request);
void			http_request_set_version(HttpRequest * request,gdouble version);

G_END_DECLS

#endif
