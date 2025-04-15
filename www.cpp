#include "picow_http/http.h"
#include "config.hpp"

static err_t simple_resp(struct http *http,const uint8_t * body, size_t body_len, bool durable)
{
	struct resp *resp = http_resp(http);
	uint32_t temp;
	err_t err;

	if ((err = http_resp_set_len(resp, body_len)) != ERR_OK) {
		HTTP_LOG_ERROR("http_resp_set_len() failed: %d", err);
		return http_resp_err(http, HTTP_STATUS_INTERNAL_SERVER_ERROR);
	}

	if ((err = http_resp_set_type_ltrl(resp, "text/plain")) != ERR_OK) {
		HTTP_LOG_ERROR("http_resp_set_type_ltrl() failed: %d", err);
		return http_resp_err(http, HTTP_STATUS_INTERNAL_SERVER_ERROR);
	}

	if ((err = http_resp_set_hdr_ltrl(resp, "Cache-Control", "no-store"))
	    != ERR_OK) {
		HTTP_LOG_ERROR("Set header Cache-Control failed: %d", err);
		return http_resp_err(http, HTTP_STATUS_INTERNAL_SERVER_ERROR);
	}

	if(body == nullptr){
		uint8_t tm;
		return http_resp_send_buf(http, &tm, 0, durable);
	}

	return http_resp_send_buf(http, body, body_len, durable);

}

static err_t redirect(struct http *http, uint uid)
{
	struct resp *resp = http_resp(http);
	uint32_t temp;
	err_t err;

	char location[6+7];
	snprintf(location,sizeof(location),"/?uid=%d",uid);

	if ((err = http_resp_set_hdr_str(resp, "Location",location)) != ERR_OK) {
		HTTP_LOG_ERROR("http_resp_set_type_ltrl() failed: %d", err);
		return http_resp_err(http, HTTP_STATUS_INTERNAL_SERVER_ERROR);
	}
	return http_resp_err(http, HTTP_STATUS_TEMPORARY_REDIRECT);
}

static unsigned int natoi(const char *s, int n)
{
	unsigned int x = 0;
    while(isdigit(s[0]) && n--)
    {
        x = x * 10 + (s[0] - '0');
        s++;
    }
    return x;
}

static err_t config_handler(struct http *http, void *p)
{
	(void)p;
	static constexpr std::size_t MAX_INT_LEN = Config::WWW_Access::MAX_MAX_STRLEN;
	struct req *req = http_req(http);

	err_t err;
	const uint8_t *query;
	size_t query_len;

	query = http_req_query(req, &query_len);
	if (query != NULL) {
		const uint8_t *val;
		size_t val_len;
		val = http_req_query_val(query, query_len, (uint8_t*)"uid", 3, &val_len);
		if (val == NULL)
			return http_resp_err(http, HTTP_STATUS_UNPROCESSABLE_CONTENT);
		uint uid = natoi((const char *)val,val_len);
		auto elem = Config::WWW_Access::uid(uid);
		if(elem == NULL)
			return http_resp_err(http, HTTP_STATUS_UNPROCESSABLE_CONTENT);

		val = http_req_query_val(query, query_len, (uint8_t*)"op", 2, &val_len);
		if (val == NULL)
			return simple_resp(http,nullptr,0,true);
		uint op = natoi((const char *)val,val_len);
		switch(op)
		{
		case 0:
		{
			char repr[3];
			repr[0] = elem->coldboot_required?'1':'0';
			repr[1] = elem->is_directory?'1':'0';
			repr[2] = elem->to_flash?'1':'0';
			return simple_resp(http,(uint8_t*)repr,3,false);
		}
		case 1:
			return simple_resp(http,(uint8_t*)elem->help,strlen(elem->help),true);
		case 2:
			return simple_resp(http,(uint8_t*)elem->name,strlen(elem->name),true);
		case 3:
		{
			char repr[MAX_INT_LEN];
			size_t len = elem->repr(elem->val,repr);
			return simple_resp(http,(uint8_t*)repr,len,false);
		}
		case 4:
		{
			val = http_req_query_val(query, query_len, (uint8_t*)"set", 3, &val_len);
			if (val == NULL)
				return http_resp_err(http, HTTP_STATUS_UNPROCESSABLE_CONTENT);
			{
				char repr[MAX_INT_LEN+10]; //extra parseable characters
				size_t minlen = std::min(sizeof(repr)-1,val_len);
				strncpy(repr,(const char*)val, minlen);
				repr[minlen] = 0;
				elem->set_repr(elem->val,repr);
				return simple_resp(http,nullptr,0,false);
			}
		}
		case 5:
		{
			elem->reset();
			return simple_resp(http,nullptr,0,false);
		}
		case 6:
		{
			Config::WWW_Access::TSaved::load();
			return simple_resp(http,nullptr,0,false);
		}
		case 7:
		{
			Config::WWW_Access::TSaved::save();
			return simple_resp(http,nullptr,0,false);
		}
		}
		return http_resp_err(http, HTTP_STATUS_UNPROCESSABLE_CONTENT);
	}
	return http_resp_err(http, HTTP_STATUS_UNPROCESSABLE_CONTENT);
}

static size_t find_idx_of_val(const uint8_t *buf,size_t len, const char *const* tbl)
{
	size_t i=0;
	for(;tbl[i][0] !=0;i++)
	{
		int tlen = strlen(tbl[i]);
		if(tlen!=len)
			continue;
		if(!memcmp(buf,tbl[i],len))
			break;
	}
	return i;
}

static err_t index_config_handler(struct http *http, void *p)
{
	(void)p;
	static constexpr std::size_t MAX_INT_LEN = Config::WWW_Access::MAX_MAX_STRLEN;
	struct req *req = http_req(http);

	err_t err;
	const uint8_t *query;
	size_t query_len;

	query = http_req_query(req, &query_len);
	if (query != NULL) {
		const uint8_t *val;
		size_t val_len;
		val = http_req_query_val(query, query_len, (uint8_t*)"uid", 3, &val_len);
		if (val == NULL)
			return http_resp_err(http, HTTP_STATUS_UNPROCESSABLE_CONTENT);
		uint uid = natoi((const char *)val,val_len);
		auto elem = Config::WWW_Access::uid(uid);
		if(elem == NULL)
			return http_resp_err(http, HTTP_STATUS_UNPROCESSABLE_CONTENT);

		val = http_req_query_val(query, query_len, (uint8_t*)"op", 2, &val_len);
		if (val == NULL)
			return simple_resp(http,nullptr,0,true);
		static constexpr const char * op_tbl[] =
		{
			"set",
			"default",
			"loadall",
			"saveall",
			""
		};
		uint op = find_idx_of_val(val,val_len, op_tbl);
		switch(op)
		{
		case 0:
		{
			val = http_req_query_val(query, query_len, (uint8_t*)"set", 3, &val_len);
			if (val == NULL)
				return http_resp_err(http, HTTP_STATUS_UNPROCESSABLE_CONTENT);
			{
				char repr[MAX_INT_LEN+10]; //extra parseable characters
				size_t minlen = std::min(sizeof(repr)-1,val_len);
				strncpy(repr,(const char*)val, minlen);
				repr[minlen] = 0;
				elem->set_repr(elem->val,repr);
				return redirect(http,uid);
			}
		}
		case 1:
		{
			elem->reset();
			return redirect(http,uid);
		}
		case 2:
		{
			Config::WWW_Access::TSaved::load();
			return redirect(http,uid);
		}
		case 3:
		{
			Config::WWW_Access::TSaved::save();
			return redirect(http,uid);
		}
		}
		return http_resp_err(http, HTTP_STATUS_UNPROCESSABLE_CONTENT);
	}
	return http_resp_err(http, HTTP_STATUS_UNPROCESSABLE_CONTENT);
}


template<size_t LEN>
static inline void putcstr(char ** place,const CStr<LEN> & src)
{
	memcpy(*place,src.data(),LEN);
	*place+=LEN;
}
static inline void putstr(char ** place,const char* src)
{
	size_t len=strlen(src);
	memcpy(*place,src,len);
	*place+=len;
}



struct index_thread_local {
	index_thread_local * prev;
	uint phase;
	const Config::WWW_Access::ConfigFields * elem;
	char repr_buf[Config::WWW_Access::MAX_MAX_STRLEN];
};

//10 max nesting
LWIP_MEMPOOL_DECLARE(index_pool, HTTP_N, sizeof(index_thread_local), "struct index");

struct index_tree {
	void * prev;
	const Config::WWW_Access::ConfigFields * elem;
	uint elem_idx;
	uint tree_phase;
	uint nest;
	uint nesti;
};

LWIP_MEMPOOL_DECLARE(index_tree_pool, Config::WWW_Access::FIELD_COUNT+2, sizeof(index_tree), "struct index_tree");


static void index_free(void *p)
{
	LWIP_MEMPOOL_FREE(index_pool, p);
}

static err_t index_continuator(http *http);

static err_t tree_handler(http *http)
{
	index_tree* thr = (index_tree*)http_cx_priv(http);
	err_t err;
	switch(thr->tree_phase)
	{
	case 0:
		thr->nesti = 0;
		thr->tree_phase = 1;
		/* no break */
	case 1:
		while(thr->nesti<thr->nest)
		{
			static const char place[] ="&emsp;";
			err = http_resp_send_buf_chain(http, (uint8_t*)place, sizeof(place)-1, true,false);
			if(err == ERR_MEM)
				return ERR_OK;
			if(err != ERR_OK)
				return err;
			thr->nesti++;
		}
		static const char place[] ="<a href=\"/?uid=";

		err = http_resp_send_buf_chain(http, (uint8_t*)place, sizeof(place)-1, true,false);
		if(err == ERR_MEM)
			return ERR_OK;
		if(err != ERR_OK)
			return err;
		thr->tree_phase++;
		/* no break */
	case 2:
	{
		char tmpbuf[6];
		snprintf(tmpbuf,sizeof(tmpbuf),"%d",thr->elem->uid);
		err = http_resp_send_buf_chain(http, (uint8_t*)tmpbuf, strlen(tmpbuf), false,false);
		if(err == ERR_MEM)
			return ERR_OK;
		if(err != ERR_OK)
			return err;
		thr->tree_phase++;
	}
	/* no break */
	case 3:
	{
		static const char place[] ="\">";
		err = http_resp_send_buf_chain(http, (uint8_t*)place, sizeof(place)-1, true,false);
		if(err == ERR_MEM)
			return ERR_OK;
		if(err != ERR_OK)
			return err;
		thr->tree_phase++;
	}
	/* no break */
	case 4:
	{

		if(thr->elem->is_directory)
		{
			static constexpr std::size_t MAX_INT_LEN = Config::WWW_Access::MAX_MAX_STRLEN;
			uint16_t tmpbuf[MAX_INT_LEN/sizeof(uint16_t)];
			size_t len = thr->elem->repr(thr->elem->val,(char*)tmpbuf)/sizeof(uint16_t);
			thr->elem_idx = len;
		}
		else
		{
			thr->elem_idx = 0;
		}
		err = http_resp_send_buf_chain(http, (uint8_t*)thr->elem->name, strlen(thr->elem->name), true,false);
		if(err == ERR_MEM)
			return ERR_OK;
		if(err != ERR_OK)
			return err;
		thr->tree_phase++;
	}
	/* no break */
	case 5:
	{
		static const char place[] ="</a><br>";
		while(thr->elem_idx == 0)
		{
			if(thr->elem->uid == 0)
			{
				err_t err = http_resp_send_buf_chain(http, (uint8_t*)place, sizeof(place)-1, true,false);
				if (err == ERR_MEM)
					return ERR_OK;
				if (err != ERR_OK)
					return err;
				http_cx_set_priv(http,thr->prev,nullptr,index_continuator);
				LWIP_MEMPOOL_FREE(index_tree_pool, thr);
				return ERR_OK;
			}
			auto * tmp = (index_tree*)thr->prev;
			LWIP_MEMPOOL_FREE(index_tree_pool, thr);
			thr = tmp;
			thr->elem_idx--;
		}
		err_t err = http_resp_send_buf_chain(http, (uint8_t*)place, sizeof(place)-1, true,false);
		if (err == ERR_MEM)
			return ERR_OK;
		if (err != ERR_OK)
			return err;

		static constexpr std::size_t MAX_INT_LEN = Config::WWW_Access::MAX_MAX_STRLEN;
		uint16_t tmpbuf[MAX_INT_LEN/sizeof(uint16_t)];
		size_t len = thr->elem->repr(thr->elem->val,(char*)tmpbuf)/sizeof(uint16_t);
		index_tree * thr2 = (index_tree *) LWIP_MEMPOOL_ALLOC(index_tree_pool);
		if(!thr2)
			return ERR_MEM;
		thr2->prev = thr;
		thr2->elem = Config::WWW_Access::uid(tmpbuf[len-thr->elem_idx]);
		thr2->nest = thr->nest+1;
		thr2->tree_phase = 0;
		http_cx_set_priv(http,thr2,nullptr,tree_handler);
		return ERR_OK;
	}
	}
	panic("buu");
	return ERR_ARG;
}

static constexpr const char * false_true[] =
{
		"false",
		"true"
};

static err_t index_continuator(http *http)
{
	index_thread_local* thr = (index_thread_local*)http_cx_priv(http);
	err_t err;
	switch (thr->phase)
	{
	case 0:
	{
		const char place[] =
			"<html>"
			"<head>"
			"<title>Picopocket Config</title>"
			"</head>"
			"<body>"
			"<h1>Picopocket Config</h1>"
			"<table border=\"1\">"
			"<tr>"
			"<td>";
		err = http_resp_send_buf_chain(http, (uint8_t*)place, sizeof(place)-1, true, false);
		if (err == ERR_MEM)
			return ERR_OK;
		if (err != ERR_OK)
			return err;
		thr->phase++;
		index_tree * thr2 = (index_tree *) LWIP_MEMPOOL_ALLOC(index_tree_pool);
		if(!thr2)
			return ERR_MEM;
		thr2->prev = thr;
		thr2->elem = Config::WWW_Access::uid(0);
		thr2->tree_phase = 0;
		thr2->nest = 0;
		http_cx_set_priv(http,thr2,nullptr,tree_handler);
		return ERR_OK;
	}
	/* no break */
	case 1:
	{
		static const char place[] =
				"</td>"
				"<td>"
				"<h2>";
		err = http_resp_send_buf_chain(http, (uint8_t*)place, sizeof(place)-1, true, false);
		if (err == ERR_MEM)
			return ERR_OK;
		if (err != ERR_OK)
			return err;
		thr->phase++;
	}
	/* no break */
	case 2:
	{
		err =  http_resp_send_buf_chain(http, (uint8_t*)thr->elem->name, strlen(thr->elem->name), true,false);
		if (err == ERR_MEM)
			return ERR_OK;
		if (err != ERR_OK)
			return err;
		thr->phase++;
	}
	/* no break */
	case 3:
	{
		static const char place[] =
			"</h2>"
			"<br>"
			"<form action=\"/index_config\">"
			"<input type=\"hidden\" name=\"uid\" value=\"";
		err = http_resp_send_buf_chain(http, (uint8_t*)place, sizeof(place)-1, true,false);
		if (err == ERR_MEM)
			return ERR_OK;
		if (err != ERR_OK)
			return err;
		thr->phase++;
	}
	/* no break */
	case 4:
	{
		char uidstr[6];
		snprintf(uidstr,sizeof(uidstr),"%d", thr->elem->uid);
		err = http_resp_send_buf_chain(http, (uint8_t*)uidstr, strlen(uidstr), false,false);
		if (err == ERR_MEM)
			return ERR_OK;
		if (err != ERR_OK)
			return err;
		thr->phase++;
	}
	/* no break */
	case 5:
	{
		static const char place[] =
				"\">"
				"<label>Value: "
				"<input type=\"text\" name=\"set\" value=\"";
		err = http_resp_send_buf_chain(http, (uint8_t*)place, sizeof(place)-1, true,false);
		if (err == ERR_MEM)
			return ERR_OK;
		if (err != ERR_OK)
			return err;
		thr->phase++;
	}
	/* no break */
	case 6:
	{
		if(!thr->elem->is_directory)
		{
			size_t len = thr->elem->repr(thr->elem->val,thr->repr_buf);
			err =  http_resp_send_buf_chain(http, (uint8_t*)thr->repr_buf, strlen(thr->repr_buf), false,false);
			if (err == ERR_MEM)
				return ERR_OK;
			if (err != ERR_OK)
				return err;
		}
		thr->phase++;
	}
	/* no break */
	case 7:
	{
		static const char place[] =
			"\">"
			"</label><br>"
			"<label>"
			"<input type=\"submit\" name=\"op\" value=\"set\">"
			"</label>"
			"<label>"
			"<input type=\"submit\" name=\"op\" value=\"default\">"
			"</label>"
			"<br>"
			"<label>"
			"<input type=\"submit\" name=\"op\" value=\"saveall\">"
			"</label>"
			"<label>"
			"<input type=\"submit\" name=\"op\" value=\"loadall\">"
			"</label>"
			"</form><br>"
			"to_flash: ";
		err = http_resp_send_buf_chain(http, (uint8_t*)place, sizeof(place)-1, true,false);
		if (err == ERR_MEM)
			return ERR_OK;
		if (err != ERR_OK)
			return err;
		thr->phase++;
	}
	/* no break */
	case 8:
	{

		const char * place = false_true[thr->elem->to_flash];
		err = http_resp_send_buf_chain(http, (uint8_t*)place, strlen(place), false,false);
		if (err == ERR_MEM)
			return ERR_OK;
		if (err != ERR_OK)
			return err;
		thr->phase++;
	}
	/* no break */
	case 9:
	{
		static const char place[] =
			"<br>"
			"coldboot_required: ";
		err = http_resp_send_buf_chain(http, (uint8_t*)place, sizeof(place)-1, true,false);
		if (err == ERR_MEM)
			return ERR_OK;
		if (err != ERR_OK)
			return err;
		thr->phase++;
	}
	/* no break */
	case 10:
	{

		const char * place = false_true[thr->elem->coldboot_required];
		err = http_resp_send_buf_chain(http, (uint8_t*)place, strlen(place), false,false);
		if (err == ERR_MEM)
			return ERR_OK;
		if (err != ERR_OK)
			return err;
		thr->phase++;
	}
	/* no break */
	case 11:
	{
		static const char place[] =
				"<br>"
				"is_directory: ";
		err = http_resp_send_buf_chain(http, (uint8_t*)place, sizeof(place)-1, true,false);
		if (err == ERR_MEM)
			return ERR_OK;
		if (err != ERR_OK)
			return err;
		thr->phase++;
	}
	/* no break */
	case 12:
	{

		const char * place = false_true[thr->elem->is_directory];
		err = http_resp_send_buf_chain(http, (uint8_t*)place, strlen(place), false,false);
		if (err == ERR_MEM)
			return ERR_OK;
		if (err != ERR_OK)
			return err;
		thr->phase++;
	}
	/* no break */
	case 13:
	{
		static const char place[] =
				"<br>";
		err = http_resp_send_buf_chain(http, (uint8_t*)place, sizeof(place)-1, true,false);
		if (err == ERR_MEM)
			return ERR_OK;
		if (err != ERR_OK)
			return err;
		thr->phase++;
	}
	/* no break */
	case 14:
	{

		const char * place = thr->elem->help;
		err = http_resp_send_buf_chain(http, (uint8_t*)place, strlen(place), false,false);
		if (err == ERR_MEM)
			return ERR_OK;
		if (err != ERR_OK)
			return err;
		thr->phase++;
	}
	/* no break */
	case 15:
	{
		static const char place[] =
			"<br>"
			"</td>"
			"</tr>"
			"</table>"
			"</body>"
			"</html>"
		;
		err_t err = http_resp_send_buf_chain(http, (uint8_t*)place, sizeof(place)-1, true, true);
		if (err == ERR_MEM)
		{
			return ERR_OK;
		}
		if (err != ERR_OK)
			return err;
		http_cx_set_priv(http,thr,index_free,nullptr);
		return ERR_OK;
	}
	};
	panic("buuuu");
}

static err_t index_handler(struct http *http, void *p)
{
	struct resp *resp = http_resp(http);
	err_t err;

	if ((err = http_resp_set_type_ltrl(resp, "text/html")) != ERR_OK) {
		HTTP_LOG_ERROR("http_resp_set_type_ltrl() failed: %d", err);
		return http_resp_err(http, HTTP_STATUS_INTERNAL_SERVER_ERROR);
	}

	if ((err = http_resp_set_hdr_ltrl(resp, "Cache-Control", "no-store"))
	    != ERR_OK) {
		HTTP_LOG_ERROR("Set header Cache-Control failed: %d", err);
		return http_resp_err(http, HTTP_STATUS_INTERNAL_SERVER_ERROR);
	}

	const uint8_t *query;
	size_t query_len;
	struct req *req = http_req(http);
	query = http_req_query(req, &query_len);

	uint16_t uid = 0;
	if (query != NULL) {
		const uint8_t *val;
		size_t val_len;
		val = http_req_query_val(query, query_len, (uint8_t*)"uid", 3, &val_len);
		if (val != NULL)
			uid = natoi((const char *)val,val_len);
	}

	auto elem = Config::WWW_Access::uid(uid);

	index_thread_local * thr = (index_thread_local *) LWIP_MEMPOOL_ALLOC(index_pool);
	if(!thr)
		return ERR_MEM;
	thr->prev = nullptr;
	thr->phase = 0;
	thr->elem = elem;

	http_cx_set_priv(http,thr,nullptr,index_continuator);
	return ERR_OK;
}

void www_init()
{
	err_t err;
	struct server_cfg cfg;
	struct server *srv;

	// Start with the default config, so that we only need to change
	// individual fields.
	cfg = http_default_cfg();

	err = register_hndlr_methods(&cfg, "/config", config_handler,
			HTTP_METHODS_GET_HEAD, nullptr);

	err = register_hndlr_methods(&cfg, "/index_config", index_config_handler,
			HTTP_METHODS_GET_HEAD, nullptr);

	LWIP_MEMPOOL_INIT(index_pool);
	LWIP_MEMPOOL_INIT(index_tree_pool);

	err = register_hndlr_methods(&cfg, "/", index_handler,
			HTTP_METHODS_GET_HEAD, nullptr);
	err = http_srv_init(&srv, &cfg);
}
