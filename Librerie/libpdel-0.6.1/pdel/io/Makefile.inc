# $Id: Makefile.inc 1032 2006-12-18 08:17:16Z mgooderum $

io_PATH+=	${PDELTOP}/io
io_SRCS+=	base64.c \
		boundary_fp.c \
		count_fp.c \
		filter.c \
        misc_io.c \
		poll.c \
        socketpair.c \
		ssl_fp.c \
		string_fp.c \
		timeout_fp.c

io_INCS+=	base64.h \
		boundary_fp.h \
		count_fp.h \
		filter.h \
		ssl_fp.h \
		string_fp.h \
		timeout_fp.h

io_MAN+=	base64.3
io_MLINKS+=	base64.3 b64_encoder_create.3
io_MLINKS+=	base64.3 b64_decoder_create.3
io_MLINKS+=	base64.3 b64_rfc2045_charset.3

io_MAN+=	boundary_fopen.3

io_MAN+=	count_fopen.3

io_MAN+=	timeout_fp.3
io_MLINKS+=	timeout_fp.3 timeout_fdopen.3

io_MAN+=	filter.3
io_MLINKS+=	filter.3 filter_read.3
io_MLINKS+=	filter.3 filter_write.3
io_MLINKS+=	filter.3 filter_end.3
io_MLINKS+=	filter.3 filter_convert.3
io_MLINKS+=	filter.3 filter_destroy.3
io_MLINKS+=	filter.3 filter_fopen.3
io_MLINKS+=	filter.3 filter_process.3

io_MAN+=	string_fp.3
io_MLINKS+=	string_fp.3 string_buf_input.3
io_MLINKS+=	string_fp.3 string_buf_output.3
io_MLINKS+=	string_fp.3 string_buf_content.3
io_MLINKS+=	string_fp.3 string_buf_length.3

io_MAN+=	ssl_fp.3
io_MLINKS+=	ssl_fp.3 ssl_fdopen.3
io_MLINKS+=	ssl_fp.3 ssl_log.3

