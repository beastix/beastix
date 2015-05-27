/*
 * This file has been modified for the cdrkit suite.
 *
 * The behaviour and appearence of the program code below can differ to a major
 * extent from the version distributed by the original author(s).
 *
 * For details, see Changelog file distributed with the cdrkit package. If you
 * received this file from another source then ask the distributing person for
 * a log of modifications.
 *
 */

/* @(#)p_block.c	1.19 04/02/23 J. Schilling from cdparanoia-III-alpha9.8 */
/*
 *	Modifications to make the code portable Copyright (c) 2002 J. Schilling
 */
#include <mconfig.h>
#include <stdxlib.h>
#include <standard.h>
#include <utypes.h>
#include <strdefs.h>
#include "p_block.h"
#include "cdda_paranoia.h"
#include "pmalloc.h"

linked_list 	*new_list(void *(*newp) (void), void (*freep) (void *));
linked_element *add_elem(linked_list *l, void *elem);
linked_element *new_elem(linked_list *list);
void 				free_elem(linked_element *e, int free_ptr);
void 				free_list(linked_list *list, int free_ptr);
void 				*get_elem(linked_element *e);
linked_list 	*copy_list(linked_list *list);
static c_block *i_cblock_constructor(void);
void 				i_cblock_destructor(c_block *c);
c_block 			*new_c_block(cdrom_paranoia *p);
void 				free_c_block(c_block *c);
static v_fragment *i_vfragment_constructor(void);
static void 	i_v_fragment_destructor(v_fragment *v);
v_fragment 		*new_v_fragment(cdrom_paranoia *p, c_block *one, long begin, 
										 long end, int last);
void 				free_v_fragment(v_fragment *v);
c_block 			*c_first(cdrom_paranoia *p);
c_block 			*c_last(cdrom_paranoia *p);
c_block 			*c_next(c_block *c);
c_block 			*c_prev(c_block *c);
v_fragment 		*v_first(cdrom_paranoia *p);
v_fragment 		*v_last(cdrom_paranoia *p);
v_fragment 		*v_next(v_fragment *v);
v_fragment 		*v_prev(v_fragment *v);
void 				recover_cache(cdrom_paranoia *p);
Int16_t 			*v_buffer(v_fragment *v);
c_block 			*c_alloc(Int16_t *vector, long begin, long size);
void 				c_set(c_block *v, long begin);
void 				c_remove(c_block *v, long cutpos, long cutsize);
void 				c_overwrite(c_block *v, long pos, Int16_t *b, long size);
void 				c_append(c_block *v, Int16_t *vector, long size);
void 				c_removef(c_block *v, long cut);
void 				i_paranoia_firstlast(cdrom_paranoia *p);
cdrom_paranoia *paranoia_init(void *d, int nsectors);


linked_list *new_list(void *(*newp)(void), void (*freep)(void *))
{
	linked_list	*ret = _pcalloc(1, sizeof (linked_list));

	ret->new_poly = newp;
	ret->free_poly = freep;
	return (ret);
}

linked_element *add_elem(linked_list *l, void *elem)
{

	linked_element	*ret = _pcalloc(1, sizeof (linked_element));

	ret->stamp = l->current++;
	ret->ptr = elem;
	ret->list = l;

	if (l->head)
		l->head->prev = ret;
	else
		l->tail = ret;
	ret->next = l->head;
	ret->prev = NULL;
	l->head = ret;
	l->active++;

	return (ret);
}

linked_element *new_elem(linked_list *list)
{
	void		*new = list->new_poly();

	return (add_elem(list, new));
}

void free_elem(linked_element *e, int free_ptr)
{
	linked_list	*l = e->list;

	if (free_ptr)
		l->free_poly(e->ptr);

	if (e == l->head)
		l->head = e->next;
	if (e == l->tail)
		l->tail = e->prev;

	if (e->prev)
		e->prev->next = e->next;
	if (e->next)
		e->next->prev = e->prev;

	l->active--;
	_pfree(e);
}

void free_list(linked_list *list, int free_ptr)
{
	while (list->head)
		free_elem(list->head, free_ptr);
	_pfree(list);
}

void *get_elem(linked_element *e)
{
	return (e->ptr);
}

linked_list *copy_list(linked_list *list)
{
	linked_list	*new = new_list(list->new_poly, list->free_poly);
	linked_element	*i = list->tail;

	while (i) {
		add_elem(new, i->ptr);
		i = i->prev;
	}
	return (new);
}

/**** C_block stuff ******************************************************/

#define	vp_cblock_constructor_func ((void*(*)(void))i_cblock_constructor)
static c_block *i_cblock_constructor()
{
	c_block		*ret = _pcalloc(1, sizeof (c_block));

	return (ret);
}

#define	vp_cblock_destructor_func ((void(*)(void*))i_cblock_destructor)
void i_cblock_destructor(c_block *c)
{
	if (c) {
		if (c->vector)
			_pfree(c->vector);
		if (c->flags)
			_pfree(c->flags);
		c->e = NULL;
		_pfree(c);
	}
}

c_block *new_c_block(cdrom_paranoia *p)
{
	linked_element	*e = new_elem(p->cache);
	c_block		*c = e->ptr;

	c->e = e;
	c->p = p;
	return (c);
}

void free_c_block(c_block *c)
{
	/*
	 * also rid ourselves of v_fragments that reference this block
	 */
	v_fragment		*v = v_first(c->p);

	while (v) {
		v_fragment	*next = v_next(v);

		if (v->one == c)
			free_v_fragment(v);
		v = next;
	}

	free_elem(c->e, 1);
}

#define	vp_vfragment_constructor_func ((void*(*)(void))i_vfragment_constructor)
static v_fragment *i_vfragment_constructor()
{
	v_fragment	*ret = _pcalloc(1, sizeof (v_fragment));

	return (ret);
}

#define	vp_v_fragment_destructor_func ((void(*)(void*))i_v_fragment_destructor)
static void i_v_fragment_destructor(v_fragment *v)
{
	_pfree(v);
}

v_fragment *new_v_fragment(cdrom_paranoia *p, c_block *one, long begin, 
                           long end, int last)
{
	linked_element	*e = new_elem(p->fragments);
	v_fragment	*b = e->ptr;

	b->e = e;
	b->p = p;

	b->one = one;
	b->begin = begin;
	b->vector = one->vector + begin - one->begin;
	b->size = end - begin;
	b->lastsector = last;

	return (b);
}

void free_v_fragment(v_fragment *v)
{
	free_elem(v->e, 1);
}

c_block *c_first(cdrom_paranoia *p)
{
	if (p->cache->head)
		return (p->cache->head->ptr);
	return (NULL);
}

c_block* c_last(cdrom_paranoia *p)
{
	if (p->cache->tail)
		return (p->cache->tail->ptr);
	return (NULL);
}

c_block *c_next(c_block *c)
{
	if (c->e->next)
		return (c->e->next->ptr);
	return (NULL);
}

c_block *c_prev(c_block *c)
{
	if (c->e->prev)
		return (c->e->prev->ptr);
	return (NULL);
}

v_fragment *v_first(cdrom_paranoia *p)
{
	if (p->fragments->head) {
		return (p->fragments->head->ptr);
	}
	return (NULL);
}

v_fragment *v_last(cdrom_paranoia *p)
{
	if (p->fragments->tail)
		return (p->fragments->tail->ptr);
	return (NULL);
}

v_fragment *v_next(v_fragment *v)
{
	if (v->e->next)
		return (v->e->next->ptr);
	return (NULL);
}

v_fragment *v_prev(v_fragment *v)
{
	if (v->e->prev)
		return (v->e->prev->ptr);
	return (NULL);
}

void recover_cache(cdrom_paranoia *p)
{
	linked_list	*l = p->cache;

	/*
	 * Are we at/over our allowed cache size?
	 */
	while (l->active > p->cache_limit) {
		/*
		 * cull from the tail of the list
		 */
		free_c_block(c_last(p));
	}

}

Int16_t *v_buffer(v_fragment *v)
{
	if (!v->one)
		return (NULL);
	if (!cv(v->one))
		return (NULL);
	return (v->vector);
}

/*
 * alloc a c_block not on a cache list
 */
c_block *c_alloc(Int16_t *vector, long begin, long size)
{
	c_block		*c = _pcalloc(1, sizeof (c_block));

	c->vector = vector;
	c->begin = begin;
	c->size = size;
	return (c);
}

void c_set(c_block *v, long begin)
{
	v->begin = begin;
}

/*
 * pos here is vector position from zero
 */
void c_insert(c_block *v, long pos, Int16_t *b, long size)
{
	int		vs = cs(v);

	if (pos < 0 || pos > vs)
		return;

	if (v->vector)
		v->vector = _prealloc(v->vector, sizeof (Int16_t) * (size + vs));
	else
		v->vector = _pmalloc(sizeof (Int16_t) * size);

	if (pos < vs)
		memmove(v->vector + pos + size, v->vector + pos,
			(vs - pos) * sizeof (Int16_t));
	memcpy(v->vector + pos, b, size * sizeof (Int16_t));

	v->size += size;
}

void c_remove(c_block *v, long cutpos, long cutsize)
{
	int		vs = cs(v);

	if (cutpos < 0 || cutpos > vs)
		return;
	if (cutpos + cutsize > vs)
		cutsize = vs - cutpos;
	if (cutsize < 0)
		cutsize = vs - cutpos;
	if (cutsize < 1)
		return;

	memmove(v->vector + cutpos, v->vector + cutpos + cutsize,
		(vs - cutpos - cutsize) * sizeof (Int16_t));

	v->size -= cutsize;
}

void c_overwrite(c_block *v, long pos, Int16_t *b, long size)
{
	int		vs = cs(v);

	if (pos < 0)
		return;
	if (pos + size > vs)
		size = vs - pos;

	memcpy(v->vector + pos, b, size * sizeof (Int16_t));
}

void c_append(c_block *v, Int16_t *vector, long size)
{
	int		vs = cs(v);

	/*
	 * update the vector
	 */
	if (v->vector)
		v->vector = _prealloc(v->vector, sizeof (Int16_t) * (size + vs));
	else
		v->vector = _pmalloc(sizeof (Int16_t) * size);
	memcpy(v->vector + vs, vector, sizeof (Int16_t) * size);

	v->size += size;
}

void c_removef(c_block *v, long cut)
{
	c_remove(v, 0, cut);
	v->begin += cut;
}



/*
 * Initialization
 */
void i_paranoia_firstlast(cdrom_paranoia *p)
{
	int	i;
	void	*d = p->d;

	p->current_lastsector = -1;
	for (i = cdda_sector_gettrack(d, p->cursor); i < cdda_tracks(d); i++)
		if (!cdda_track_audiop(d, i))
			p->current_lastsector = cdda_track_lastsector(d, i - 1);
	if (p->current_lastsector == -1)
		p->current_lastsector = cdda_disc_lastsector(d);

	p->current_firstsector = -1;
	for (i = cdda_sector_gettrack(d, p->cursor); i > 0; i--)
		if (!cdda_track_audiop(d, i))
			p->current_firstsector = cdda_track_firstsector(d, i + 1);
	if (p->current_firstsector == -1)
		p->current_firstsector = cdda_disc_firstsector(d);

}

cdrom_paranoia *paranoia_init(void *d, int nsectors)
{
	cdrom_paranoia	*p = _pcalloc(1, sizeof (cdrom_paranoia));

	p->cache = new_list(vp_cblock_constructor_func,
				vp_cblock_destructor_func);

	p->fragments = new_list(vp_vfragment_constructor_func,
				vp_v_fragment_destructor_func);

	p->nsectors  = nsectors;
	p->readahead = 150;
	p->sortcache = sort_alloc(p->readahead * CD_FRAMEWORDS);
	p->d = d;
	p->mindynoverlap = MIN_SECTOR_EPSILON;
	p->maxdynoverlap = MAX_SECTOR_OVERLAP * CD_FRAMEWORDS;
	p->maxdynoverlap = (nsectors - 1) * CD_FRAMEWORDS;
	p->dynoverlap = MAX_SECTOR_OVERLAP * CD_FRAMEWORDS;
	p->cache_limit = JIGGLE_MODULO;
	p->enable = PARANOIA_MODE_FULL;
	p->cursor = cdda_disc_firstsector(d);
	p->lastread = -1000000;

	/*
	 * One last one... in case data and audio tracks are mixed...
	 */
	i_paranoia_firstlast(p);

	return (p);
}

void paranoia_dynoverlapset(cdrom_paranoia *p, int minoverlap, int maxoverlap)
{
	if (minoverlap >= 0)
		p->mindynoverlap = minoverlap;
	if (maxoverlap > minoverlap)
		p->maxdynoverlap = maxoverlap;

	if (p->maxdynoverlap < p->mindynoverlap)
		p->maxdynoverlap = p->mindynoverlap;
}
