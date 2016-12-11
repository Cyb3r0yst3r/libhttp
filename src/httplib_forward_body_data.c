/* 
 * Copyright (c) 2016 Lammert Bies
 * Copyright (c) 2013-2016 the Civetweb developers
 * Copyright (c) 2004-2013 Sergey Lyubka
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */



#include "libhttp-private.h"



/*
 * int XX_httplib_forward_body_data( struct mg_connection *conn, FILE *fp, SOCKET sock, SSL *ssl );
 *
 * The function XX_httplib_forward_body_data() forwards body data to the
 * client.
 */

#if !defined(NO_CGI) || !defined(NO_FILES)
int XX_httplib_forward_body_data( struct mg_connection *conn, FILE *fp, SOCKET sock, SSL *ssl ) {

	const char *expect;
	const char *body;
	char buf[MG_BUF_LEN];
	int to_read;
	int nread;
	int success = 0;
	int64_t buffered_len;
	double timeout = -1.0;

	if (!conn) { return 0; }
	if (conn->ctx->config[REQUEST_TIMEOUT]) {
		timeout = atoi(conn->ctx->config[REQUEST_TIMEOUT]) / 1000.0;
	}

	expect = mg_get_header(conn, "Expect");
	/* assert(fp != NULL); */
	if (!fp) {
		XX_httplib_send_http_error(conn, 500, "%s", "Error: NULL File");
		return 0;
	}

	if (conn->content_len == -1 && !conn->is_chunked) {
		/* Content length is not specified by the client. */
		XX_httplib_send_http_error(conn, 411, "%s", "Error: Client did not specify content length");
	} else if ((expect != NULL)
	           && (mg_strcasecmp(expect, "100-continue") != 0)) {
		/* Client sent an "Expect: xyz" header and xyz is not 100-continue. */
		XX_httplib_send_http_error(conn, 417, "Error: Can not fulfill expectation %s", expect);
	} else {
		if (expect != NULL) {
			(void)mg_printf(conn, "%s", "HTTP/1.1 100 Continue\r\n\r\n");
			conn->status_code = 100;
		} else conn->status_code = 200;

		buffered_len = (int64_t)(conn->data_len) - (int64_t)conn->request_len
		               - conn->consumed_content;

		/* assert(buffered_len >= 0); */
		/* assert(conn->consumed_content == 0); */

		if ((buffered_len < 0) || (conn->consumed_content != 0)) {
			XX_httplib_send_http_error(conn, 500, "%s", "Error: Size mismatch");
			return 0;
		}

		if (buffered_len > 0) {
			if ((int64_t)buffered_len > conn->content_len) {
				buffered_len = (int)conn->content_len;
			}
			body = conn->buf + conn->request_len + conn->consumed_content;
			XX_httplib_push_all(conn->ctx, fp, sock, ssl, body, (int64_t)buffered_len);
			conn->consumed_content += buffered_len;
		}

		nread = 0;
		while (conn->consumed_content < conn->content_len) {
			to_read = sizeof(buf);
			if ((int64_t)to_read > conn->content_len - conn->consumed_content) {
				to_read = (int)(conn->content_len - conn->consumed_content);
			}
			nread = XX_httplib_pull(NULL, conn, buf, to_read, timeout);
			if (nread <= 0 || XX_httplib_push_all(conn->ctx, fp, sock, ssl, buf, nread) != nread) break;
			conn->consumed_content += nread;
		}

		if (conn->consumed_content == conn->content_len) success = (nread >= 0);

		/* Each error code path in this function must send an error */
		if (!success) {
			/* NOTE: Maybe some data has already been sent. */
			/* TODO (low): If some data has been sent, a correct error
			 * reply can no longer be sent, so just close the connection */
			XX_httplib_send_http_error(conn, 500, "%s", "");
		}
	}

	return success;

}  /* XX_httplib_forward_body_data */
#endif