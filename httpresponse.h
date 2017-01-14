#ifndef _HTTP_RESPONSE_
#define _HTTP_RESPONSE_

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
