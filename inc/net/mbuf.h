/*
 * mbuf.h - buffer management for network packets
 *
 * TODO: Maybe consider adding refcounts to mbuf's. Let's wait until this turns
 * out to be necessary.
 */

#pragma once

#include <base/stddef.h>
#include <base/assert.h>
#include <iokernel/queue.h>

#define MBUF_DEFAULT_LEN	2048
#define MBUF_DEFAULT_HEADROOM	128

struct mbuf {
	struct mbuf	*next;	   /* the next mbuf in a chain */
	unsigned char	*head;	   /* start of the buffer */
	unsigned char	*data;	   /* current position within the buffer */
	unsigned int	head_len;  /* length of the entire buffer from @head */
	unsigned int	len;	   /* length of the data */
	unsigned int	csum_type; /* type of checksum */
	unsigned int	csum;	   /* 16-bit one's complement */

	union {
		unsigned int	txflags;  /* TX offload flags */
		unsigned int	rss_hash; /* RSS 5-tuple hash from HW */
	};
	unsigned int	pad;

	unsigned long	release_data; /* private data for the release method */
	void		(*release)(struct mbuf *m); /* frees the mbuf */
};

static inline unsigned char *__mbuf_pull(struct mbuf *m, unsigned int len)
{
	unsigned char *tmp = m->data;
	m->len -= len;
	m->data += len;
	return tmp;
}

/**
 * mbuf_pull - strips data from the beginning of the buffer
 * @m: the packet
 * @len: the length in bytes to strip
 *
 * Returns the previous start of the buffer. 
 */
static inline unsigned char *mbuf_pull(struct mbuf *m, unsigned int len)
{
	BUG_ON(len > m->len);
	return __mbuf_pull(m, len);
}

/**
 * mbuf_pull_or_null - strips data from the beginning of the buffer
 * @m: the packet
 * @len: the length in bytes to strip
 *
 * Returns the previous start of the buffer or NULL if the buffer is smaller
 * than @len.
 */
static inline unsigned char *mbuf_pull_or_null(struct mbuf *m, unsigned int len)
{
	return m->len >= len ? __mbuf_pull(m, len) : NULL;
}

/**
 * mbuf_push - prepends data to the beginning of the buffer
 * @m: the packet
 * @len: the length in bytes to prepend
 *
 * Returns the new start of the buffer. 
 */
static inline unsigned char *mbuf_push(struct mbuf *m, unsigned int len)
{
	m->data -= len;
	BUG_ON(m->data < m->head);
	m->len += len;
	return m->data;
}

/**
 * mbuf_put - appends data to the end of the buffer
 * @m: the packet
 * @len: the length in bytes to append
 *
 * Returns the previous end of the buffer. 
 */
static inline unsigned char *mbuf_put(struct mbuf *m, unsigned int len)
{
	unsigned char *tmp = m->data + m->len;
	m->len += len;
	BUG_ON(m->len > m->head_len);
	return tmp;
}

/**
 * mbuf_trim - strips data off the end of the buffer
 * @m: the packet
 * @len: the length in bytes to strip
 *
 * Returns a pointer to the start of the bytes that were stripped.
 */
static inline unsigned char *mbuf_trim(struct mbuf *m, unsigned int len)
{
	BUG_ON(len > m->len);
	m->len -= len;
	return m->data + m->len;
}

/**
 * mbuf_headroom - returns the space available before the start of the buffer
 * @m: the packet
 */
static inline unsigned int mbuf_headroom(struct mbuf *m)
{
	return m->data - m->head;
}

/**
 * mbuf_tailroom - returns the space available after the end of the buffer
 * @m: the packet
 */
static inline unsigned int mbuf_tailroom(struct mbuf *m)
{
	return m->head + m->head_len - m->data - m->len;
}

/**
 * mbuf_data - returns the current data pointer
 * @m: the packet
 */
static inline unsigned char *mbuf_data(struct mbuf *m)
{
	return m->data;
}

/**
 * mbuf_length - returns the current data length
 * @m: the packet
 */
static inline unsigned int mbuf_length(struct mbuf *m)
{
	return m->len;
}

/*
 * These marcos automatically typecast and determine the size of header structs.
 * In most situations you should use these instead of the raw ops above.
 */
#define mbuf_pull_hdr(mbuf, hdr) \
	(typeof(hdr) *)mbuf_pull(mbuf, sizeof(hdr))

#define mbuf_pull_hdr_or_null(mbuf, hdr) \
	(typeof(hdr) *)mbuf_pull_or_null(mbuf, sizeof(hdr))

#define mbuf_push_hdr(mbuf, hdr) \
	(typeof(hdr) *)mbuf_push(mbuf, sizeof(hdr))

#define mbuf_put_hdr(mbuf, hdr) \
	(typeof(hdr) *)mbuf_put(mbuf, sizeof(hdr))

#define mbuf_trim_hdr(mbuf, hdr) \
	(typeof(hdr) *)mbuf_trim(mbuf, sizeof(hdr))

/**
 * mbuf_init - initializes an mbuf
 * @m: the packet to initialize
 * @head: the start of the backing buffer
 * @head_len: the length of backing buffer
 * @reserve_len: the number of bytes to reserve at the start of @head
 */
static inline void mbuf_init(struct mbuf *m, unsigned char *head,
			     unsigned int head_len, unsigned int reserve_len)
{
	assert(reserve_len < head_len);
	m->head = head;
	m->head_len = head_len;
	m->data = m->head + reserve_len;
	m->len = 0;
}

/**
 * mbuf_free - frees an mbuf back to an allocator
 * @m: the mbuf to free
 */
static inline void mbuf_free(struct mbuf *m)
{
	m->release(m);
}

extern struct mbuf *mbuf_clone(struct mbuf *dst, struct mbuf *src);
