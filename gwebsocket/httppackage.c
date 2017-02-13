#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "httppackage.h"

typedef struct _HttpPackageAttribute	HttpPackageAttribute;
typedef struct _HttpPackagePrivate	HttpPackagePrivate;

struct _HttpPackageAttribute
{
	gchar	* name,
		* value;
	gsize	length;
};

struct _HttpPackagePrivate
{
	GList * attributes;
};

G_DEFINE_TYPE_WITH_PRIVATE(HttpPackage,http_package,G_TYPE_OBJECT);

gboolean	
_http_package_read_from_stream_real(HttpPackage * package,
				GDataInputStream * data_stream,
				gsize * length,
				GCancellable * cancellable,
				GError ** error)
{
	http_package_reset(package);
	gsize
	count = 0;
	gchar
	* content = NULL;
	gboolean
	can_continue = TRUE;
	do
	{
		content = g_data_input_stream_read_line(data_stream,&count,cancellable,error);
		if(content != NULL)
		{
			can_continue = count > 0;
			if(can_continue)
			{
				gchar
				** split_content = g_strsplit(content,":",2);
				if(split_content[0] && split_content[1])
				{
					g_strstrip(split_content[0]);
					g_strstrip(split_content[1]);
					
					http_package_set_string(package,split_content[0],split_content[1],-1);
				}
				g_strfreev(split_content);
			}
			g_free(content);
		}
		else
			can_continue = FALSE;
		
	}while(can_continue);
	return TRUE;
}

gboolean	
_http_package_write_to_stream_real(HttpPackage * package,
				GOutputStream * stream,
				gsize * length,
				GCancellable * cancellable,
				GError ** error)
{
	HttpPackagePrivate
	* priv = http_package_get_instance_private(package);
	gsize 
	count = 0,total_count = 0;
	GList
	* iter;
	gboolean
	done = TRUE;
	for(iter = g_list_first(priv->attributes);((iter)&&(done));iter = iter->next)
	{
		HttpPackageAttribute
		* attr = (HttpPackageAttribute*)iter->data;
		done = g_output_stream_printf(stream,&count,cancellable,error,"%s: %s\r\n",attr->name,attr->value);
		total_count += count;
	}
	if(done)
	{
		done = g_output_stream_printf(stream,&count,cancellable,error,"\r\n");
		if(length) *length = total_count + count;
	}
	if(done)
		g_output_stream_flush(stream,cancellable,error);
	return done;
}

void
_http_package_finalize(GObject * object)
{
	http_package_reset(HTTP_PACKAGE(object));
	G_OBJECT_CLASS(http_package_parent_class)->finalize(object);
}

void
http_package_init(HttpPackage * self)
{
	HttpPackagePrivate 
	*priv = http_package_get_instance_private(self);
	priv->attributes = NULL;
}

void
http_package_class_init(HttpPackageClass * klass)
{
	klass->read_from_stream = _http_package_read_from_stream_real;
	klass->write_to_stream = _http_package_write_to_stream_real;
	G_OBJECT_CLASS(klass)->finalize = _http_package_finalize;
}


gboolean	
http_package_read_from_stream(HttpPackage * package,
				GDataInputStream * stream,
				gsize * length,
				GCancellable * cancellable,
				GError ** error)
{
	HttpPackageClass * klass = HTTP_PACKAGE_GET_CLASS(package);
	return klass->read_from_stream(package,stream,length,cancellable,error);
}

gboolean	
http_package_write_to_stream(HttpPackage * package,
				GOutputStream * stream,
				gsize * length,
				GCancellable * cancellable,
				GError ** error)
{
	HttpPackageClass * klass = HTTP_PACKAGE_GET_CLASS(package);
	return klass->write_to_stream(package,stream,length,cancellable,error);
}

void
_http_package_attribute_free(HttpPackageAttribute * attribute)
{
	g_clear_pointer(&(attribute->name),g_free);
	g_clear_pointer(&(attribute->value),g_free);
	g_free(attribute);
}

void		
http_package_reset(HttpPackage * self)
{
	HttpPackagePrivate 
	*priv = http_package_get_instance_private(self);
	g_list_free_full(priv->attributes,(GDestroyNotify)_http_package_attribute_free);
	
}

gint		
http_package_get_int(HttpPackage * package,const gchar * name)
{
	const gchar
	* value = http_package_get_string(package,name,NULL);
	if(value)
		return atoi(value);
	else
		return 0;
}

void		
http_package_set_int(HttpPackage * package,const gchar * name,gint value)
{
	static gchar string_int[100];
	sprintf(string_int,"%d",value);
	http_package_set_string(package,name,string_int,-1);
}

gint64		
http_package_get_int64(HttpPackage * package,const gchar * name)
{
	const gchar
	* value = http_package_get_string(package,name,NULL);
	if(value)
		return atol(value);
	else
		return 0L;
}

void		
http_package_set_int64(HttpPackage * package,const gchar * name,gint64 value)
{
	static gchar string_int64[100];
	sprintf(string_int64,"%lld",value);
	http_package_set_string(package,name,string_int64,-1);
}

gfloat		
http_package_get_float(HttpPackage * package,const gchar * name)
{
	const gchar
	* value = http_package_get_string(package,name,NULL);
	if(value)
		return atof(value);
	else
		return 0.0f;
}

void		
http_package_set_float(HttpPackage * package,const gchar * name,gfloat value)
{
	static gchar string_float[100];
	sprintf(string_float,"%0.2f",value);
	http_package_set_string(package,name,string_float,-1);
}

gdouble		
http_package_get_double(HttpPackage * package,const gchar * name)
{
	const gchar
	* value = http_package_get_string(package,name,NULL);
	if(value)
		return atof(value);
	else
		return 0.0;
}

void		
http_package_set_double(HttpPackage * package,const gchar * name,gdouble value)
{
	static gchar string_double[100];
	sprintf(string_double,"%g",value);
	http_package_set_string(package,name,string_double,-1);
}

const gchar *	
http_package_get_string(HttpPackage * package,const gchar * name,gsize * length)
{
	HttpPackagePrivate
	* priv = http_package_get_instance_private(package);
	HttpPackageAttribute 
	*attribute = NULL;
	GList
	* iter;
	for(iter = g_list_first(priv->attributes);iter;iter = iter->next)
	{
		HttpPackageAttribute
		* attr = (HttpPackageAttribute*)iter->data;
		if(g_ascii_strcasecmp(attr->name,name) == 0)
		{
			attribute = attr;
			break;
		}
	}
	if(attribute)
	{
		if(length)
			*length = attribute->length;
		return attribute->value;
	}
	return "";
}

void		
http_package_set_string(HttpPackage * package,
			const gchar * name,
			const gchar * value,
			gsize length)
{
	HttpPackagePrivate
	* priv = http_package_get_instance_private(package);
	HttpPackageAttribute 
	*attribute = NULL;
	GList
	* iter;
	for(iter = g_list_first(priv->attributes);iter;iter = iter->next)
	{
		HttpPackageAttribute
		* attr = (HttpPackageAttribute*)iter->data;
		if(g_ascii_strcasecmp(attr->name,name) == 0)
		{
			attribute = attr;
			break;
		}
	}
	if(!attribute)
	{
		attribute = g_new(HttpPackageAttribute,1);
		attribute->name = g_strdup(name);
		attribute->value = NULL;
		priv->attributes = g_list_append(priv->attributes,attribute);
	}
	if((int)length >= 0x0)
		attribute->length = length;
	else
		attribute->length = strlen(value);
	g_clear_pointer(&(attribute->value),g_free);
	attribute->value = g_strndup(value,attribute->length);	
}

gboolean	
http_package_is_set(HttpPackage * package,const gchar * name)
{
	HttpPackagePrivate
	* priv = http_package_get_instance_private(package);
	GList
	* iter;
	for(iter = g_list_first(priv->attributes);iter;iter = iter->next)
	{
		HttpPackageAttribute
		* attr = (HttpPackageAttribute*)iter->data;
		if(g_ascii_strcasecmp(attr->name,name) == 0)
			return TRUE;
	}
	return FALSE;
}

void		
http_package_unset(HttpPackage * package,const gchar * name)
{
	HttpPackagePrivate
	* priv = http_package_get_instance_private(package);
	HttpPackageAttribute 
	*attr = NULL;
	GList
	* iter;
	for(iter = g_list_first(priv->attributes);iter;iter = iter->next)
	{
		attr = (HttpPackageAttribute*)iter->data;
		if(g_ascii_strcasecmp(attr->name,name) == 0)
		{
			priv->attributes = g_list_remove_link(priv->attributes,iter);
			_http_package_attribute_free(attr);
			g_list_free(iter);
			return;
		}
	}
}

GDataInputStream *
http_data_input_stream(GInputStream * stream,gsize * length,GCancellable * cancellable,GError ** error)
{
	GMemoryOutputStream
	* out = G_MEMORY_OUTPUT_STREAM(g_memory_output_stream_new_resizable());
	gchar * content = NULL;
	guint8 byte = 0;
	while(g_input_stream_read(stream,&byte,1,cancellable,error))
	{
		if(g_memory_output_stream_get_data_size(out) > 2048)
		{
			g_output_stream_close(G_OUTPUT_STREAM(out),NULL,NULL);
			g_free(g_memory_output_stream_steal_data(out));
			g_object_unref(out);
			return NULL;
		}
		g_output_stream_write_all(G_OUTPUT_STREAM(out),&byte,1,NULL,NULL,NULL);
		if(g_memory_output_stream_get_data_size(out) >= 4)
		{
			content = g_memory_output_stream_get_data(out);
			if(strncmp((content + g_memory_output_stream_get_data_size(out) - 4),"\r\n\r\n",4) == 0)
				break;
		}
	}
	g_output_stream_close(G_OUTPUT_STREAM(out),NULL,NULL);
	GMemoryInputStream
	* result = G_MEMORY_INPUT_STREAM(g_memory_input_stream_new_from_data(
				g_memory_output_stream_steal_data(out),
				g_memory_output_stream_get_data_size(out),
				g_free
			));
	g_object_unref(out);
	GDataInputStream
	* dis = g_data_input_stream_new(G_INPUT_STREAM(result));
	g_data_input_stream_set_newline_type(dis,G_DATA_STREAM_NEWLINE_TYPE_CR_LF);
	return dis;
}


gchar *		
http_string_encode(const gchar * str1,gsize length)
{
	int realsize = length;
	if( realsize < 0)
		realsize = strlen(str1);
	const gchar
	* iter = NULL;
	const gchar
	* end = (str1 + realsize);
	gsize new_size = 0;
	for(iter = str1;iter < end;iter++)
	{
		if(isalnum(*iter))
			new_size ++;
		else
			new_size += 3;
	}
	gchar
	* new_str = g_new0(gchar,new_size+1),
	* new_iter = new_str;
	for(iter = str1;iter < end;iter++)
	{
		if(isgraph(*iter) || (*iter == ' ') || (*iter == '\t'))
		{
			sprintf(new_iter,"%c",*iter);
			new_iter ++;
		}
		else
		{
			sprintf(new_iter,"%%%02X",(guchar)*iter);
			new_iter+= 3;
		}
	}
	return new_str;
}

gchar *		
http_string_decode(const gchar * str1,gsize length)
{
	int realsize = length;
	if( realsize < 0)
		realsize = strlen(str1);
	gchar
	* new_temp = g_strndup(str1,realsize),
	* temp_iter = new_temp;
	const gchar
	* iter = NULL;
	const gchar
	* end = (str1 + realsize);
	gsize new_size = 0;
	for(iter = str1;iter < end;iter++)
	{
		if(*iter == '%')
		{
			int c;
			sscanf(iter+1,"%02X",&c);
			iter += 2;
			sprintf(temp_iter,"%c",c);
		}
		else
			sprintf(temp_iter,"%c",*iter);
		temp_iter ++;
		new_size ++;	
	}
	return (gchar*)g_realloc(new_temp,new_size);
}
