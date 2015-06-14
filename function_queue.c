
/*
 * Copyright 2015 Brandon Yannoni
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <errno.h>
#include <stdlib.h>
#include <pthread.h>
#include <assert.h>

#include "function_queue.h"
#include "pt_error.h"

static pthread_mutexattr_t attr;
static pthread_once_t init_attr_control = PTHREAD_ONCE_INIT;

static struct {
	int init;
	int settype;
} init_attr_ret;

static void
init_attr(void)
{
	init_attr_ret.init = pthread_mutexattr_init(&attr);

	if(init_attr_ret.init == 0)
		init_attr_ret.settype = pthread_mutexattr_settype(&attr,
				PTHREAD_MUTEX_RECURSIVE);
}

enum pt_error
fq_init(struct function_queue* q, unsigned max_elements)
{
	enum pt_error ret = PT_SUCCESS;
	int po = pthread_once(&init_attr_control, init_attr);

	if(po == 0) {
		int pml = 0;

		if(init_attr_ret.init != 0) {
			ret = PT_EPML;
		} else if(init_attr_ret.settype != 0) {
			ret = PT_EPMAI;
		} else if((pml = pthread_mutex_init(&q->lock, &attr)) == 0) {
			q->front = 0;
			q->back = 0;
			q->size = 0;
			q->max_elements = max_elements;
			q->elements = malloc(q->max_elements *
					sizeof(struct function_queue_element));

			if(q->elements == NULL) {
				ret = PT_EMALLOC;
				pml = pthread_mutex_destroy(&q->lock);

				if(pml != 0)
					ret = PT_EPMU;
			}
		} else {
			ret = PT_EPML;
		}
	} else {
		ret = PT_EPO;
	}

	return ret;
}

enum pt_error
fq_destroy(struct function_queue* q)
{
	enum pt_error ret = PT_SUCCESS;
	int pmd = pthread_mutex_destroy(&q->lock);

	if(pmd == 0) {
		free((struct function_queue_element*) q->elements);
		q->elements = NULL;
	} else {
		ret = PT_EPMD;
	}

	return ret;
}

enum pt_error
fq_push(struct function_queue* q, struct function_queue_element e, int block)
{
	enum pt_error ret = PT_SUCCESS;
	int pml = 0;

	if(block != 0)
		pml = pthread_mutex_lock(&q->lock);
	else
		pml = pthread_mutex_trylock(&q->lock);

	if(pml == 0) {
		int is_full = 0;

		fq_is_full(q, &is_full, block);
		if(is_full != 0) { /* overflow */
			ret = PT_EFQFULL;
		} else {
			++q->size;

			if(++q->back == q->max_elements)
				q->back = 0;

			q->elements[q->back] = e;
		}

		pml = pthread_mutex_unlock(&q->lock);

		if(pml != 0)
			ret = PT_EPMU;
	} else {
		if(block == 0)
			ret = PT_EPMTL;
		else
			ret = PT_EPML;
	}

	return ret;
}

enum pt_error
fq_pop(struct function_queue* q, struct function_queue_element* e, int block)
{
	enum pt_error ret = PT_SUCCESS;
	int pml = 0;

	if(block != 0)
		pml = pthread_mutex_lock(&q->lock);
	else
		pml = pthread_mutex_trylock(&q->lock);

	if(pml == 0) {
		int is_empty = 0;

		fq_is_empty(q, &is_empty, block);

		if(is_empty != 0) { /* underflow */
			ret = PT_EFQEMPTY;
		} else {
			--q->size;

			if(++q->front == q->max_elements)
				q->front = 0;

			*e = q->elements[q->front];
		}

		pml = pthread_mutex_unlock(&q->lock);

		if(pml != 0)
			ret = PT_EPMU;
	} else {
		if(block == 0)
			ret = PT_EPMTL;
		else
			ret = PT_EPML;
	}

	return ret;
}

enum pt_error
fq_peek(struct function_queue* q, struct function_queue_element* e, int block)
{
	enum pt_error ret = PT_SUCCESS;
	int pml = 0;

	if(block != 0)
		pml = pthread_mutex_lock(&q->lock);
	else
		pml = pthread_mutex_trylock(&q->lock);

	if(ret == 0) {
		int is_empty = 0;

		fq_is_empty(q, &is_empty, block);

		if(is_empty != 0) {
			ret = PT_EFQEMPTY;
		} else {
			unsigned tmp = q->front + 1;

			if(tmp == q->max_elements)
				tmp = 0;

			*e = q->elements[tmp];
		}

		pml = pthread_mutex_unlock(&q->lock);

		if(pml != 0)
			ret = PT_EPMU;
	} else {
		if(block == 0)
			ret = PT_EPMTL;
		else
			ret = PT_EPML;
	}

	return ret;
}

enum pt_error
fq_is_empty(struct function_queue* q, int* is_empty, int block)
{
	enum pt_error ret = PT_SUCCESS;
	int pml = 0;

	assert(is_empty != NULL);

	if(block != 0)
		pml = pthread_mutex_lock(&q->lock);
	else
		pml = pthread_mutex_trylock(&q->lock);

	if(pml == 0) {
		*is_empty = q->size == 0;
		pml = pthread_mutex_unlock(&q->lock);

		if(pml != 0)
			ret = PT_EPMU;
	} else {
		if(block == 0)
			ret = PT_EPMTL;
		else
			ret = PT_EPML;
	}

	return ret;
}

enum pt_error
fq_is_full(struct function_queue* q, int* is_empty, int block)
{
	enum pt_error ret = PT_SUCCESS;
	int pml = 0;

	if(block != 0)
		pml = pthread_mutex_lock(&q->lock);
	else
		pml = pthread_mutex_trylock(&q->lock);

	if(ret == 0) {
		*is_empty = q->size == q->max_elements;
		
		pml = pthread_mutex_unlock(&q->lock);

		if(pml != 0)
			ret = PT_EPMU;
	} else {
		if(block == 0)
			ret = PT_EPMTL;
		else
			ret = PT_EPML;
	}

	return ret;
}

