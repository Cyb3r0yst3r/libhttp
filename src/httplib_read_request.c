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
 * int XX_httplib_read_request( FILE *fp, struct mg_connection *conn, char *buf, int bufsiz, int *nread );
 *
 * The function XX_httplib_read_request() keeps reading the input (which can
 * either be an opened file descriptor, a socket sock or an SSL descriptor ssl)
 * into buffer buf, until \r\n\r\n appears in the buffer which marks the end
 * of the HTTP request. The buffer buf may already have some data. The length
 * of the data is stored in nread. Upon every read operation the value of nread
 * is incremented by the number of bytes read.
 */

int XX_httplib_read_request( FILE *fp, struct mg_connection *conn, char *buf, int bufsiz, int *nread ) {

	int request_len;
	int n = 0;
	struct timespec last_action_time;
	double request_timeout;

	if (!conn) return 0;

	memset(&last_action_time, 0, sizeof(last_action_time));

	if (conn->ctx->config[REQUEST_TIMEOUT]) {
		/* value of request_timeout is in seconds, config in milliseconds */
		request_timeout = atof(conn->ctx->config[REQUEST_TIMEOUT]) / 1000.0;
	} else request_timeout = -1.0;

	request_len = XX_httplib_get_request_len(buf, *nread);

	/* first time reading from this connection */
	clock_gettime(CLOCK_MONOTONIC, &last_action_time);

	while (
	    (conn->ctx->stop_flag == 0) && (*nread < bufsiz) && (request_len == 0)
	    && ((XX_httplib_difftimespec(&last_action_time, &(conn->req_time))
	         <= request_timeout) || (request_timeout < 0))
	    && ((n = XX_httplib_pull(fp, conn, buf + *nread, bufsiz - *nread, request_timeout))
	        > 0)) {
		*nread += n;
		/* assert(*nread <= bufsiz); */
		if (*nread > bufsiz) {
			return -2;
		}
		request_len = XX_httplib_get_request_len(buf, *nread);
		if (request_timeout > 0.0) clock_gettime(CLOCK_MONOTONIC, &last_action_time);
	}

	return ((request_len <= 0) && (n <= 0)) ? -1 : request_len;

}  /* XX_httplib_read_request */