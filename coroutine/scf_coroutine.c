#include"scf_coroutine.h"

scf_co_thread_t* __co_thread = NULL;

int  __scf_co_task_run(scf_co_task_t* task);
void __asm_co_task_yield(scf_co_task_t* task, uintptr_t* task_rip, uintptr_t* task_rsp, uintptr_t task_rsp0);

int64_t scf_gettime()
{
	struct timeval tv;

	gettimeofday(&tv, NULL);

	int64_t time = tv.tv_sec * 1000LL * 1000LL + tv.tv_usec;

	return time;
}

int scf_co_thread_open (scf_co_thread_t** pthread)
{
	scf_co_thread_t* thread = calloc(1, sizeof(scf_co_thread_t));
	if (!thread)
		return -ENOMEM;

	thread->epfd = epoll_create(1024);
	if (-1 == thread->epfd) {
		scf_loge("errno: %d\n", errno);
		return -1;
	}

	scf_rbtree_init(&thread->timers);
	scf_list_init  (&thread->tasks);

	__co_thread = thread;

	*pthread = thread;
	return 0;
}

int scf_co_thread_close(scf_co_thread_t*  thread)
{
	scf_loge("\n");
	return -1;
}

int scf_co_task_alloc(scf_co_task_t** ptask, uintptr_t funcptr, const char* fmt, uintptr_t rdx, uintptr_t rcx, uintptr_t r8, uintptr_t r9, uintptr_t* rsp,
		double xmm0, double xmm1, double xmm2, double xmm3, double xmm4, double xmm5, double xmm6, double xmm7)
{
	scf_co_task_t* task = calloc(1, sizeof(scf_co_task_t));
	if (!task)
		return -ENOMEM;

	const char* p;

	int i = 0;
	int j = 0;

	scf_logi("fmt: %s\n", fmt);

	for (p = fmt; *p; p++) {

		if ('d'== *p) // 'd' in format string for 'long', 'f' for 'double'
			i++;
		else if ('f' == *p)
			j++;
	}

	if (j > 0)
		task->n_floats = 1;
	else
		task->n_floats = 0;

	scf_logi("n_floats: %d\n", task->n_floats);

	i -= 4; // rdx, rcx, r8, r9 not in stack, xmm0-xmm7 too.
	j -= 8;

	if (i < 0)
		i = 0;

	if (j < 0)
		j = 0;

	int n = 6 + 8 + i + j;
	int k;

	if (n % 2 == 0)
		n++;

	task->stack_data = calloc(n, sizeof(uintptr_t));
	if (!task->stack_data) {
		free(task);
		return -ENOMEM;
	}

	scf_logi("int: %d, float: %d, n: %d\n", i, j, n);

	task->stack_len = n * sizeof(uintptr_t);

	task->stack_data[0]  = rdx;
	task->stack_data[1]  = rcx;
	task->stack_data[2]  = r8;
	task->stack_data[3]  = r9;

	task->stack_data[6]  = *(uint64_t*)&xmm0;
	task->stack_data[7]  = *(uint64_t*)&xmm1;
	task->stack_data[8]  = *(uint64_t*)&xmm2;
	task->stack_data[9]  = *(uint64_t*)&xmm3;
	task->stack_data[10] = *(uint64_t*)&xmm4;
	task->stack_data[11] = *(uint64_t*)&xmm5;
	task->stack_data[12] = *(uint64_t*)&xmm6;
	task->stack_data[13] = *(uint64_t*)&xmm7;

	i = 0;
	j = 0;
	k = 0;
	n = 14;
	for (p = fmt; *p; p++) {

		if ('d'== *p) {

			if (4 == i || 5 == i)
				task->stack_data[i] = rsp[k++];
			else if (i >= 6)
				task->stack_data[n++] = rsp[k++];
			i++;

		} else if ('f' == *p) {
			if (j >= 8)
				task->stack_data[n++] = rsp[k++];
			j++;
		}
	}

	i = 0;
	j = 0;
	for (p = fmt; *p; p++) {

		if ('d'== *p) {
			scf_logi("d, task->stack[%d]: %#lx\n", i, task->stack_data[i]);
			i++;
		} else if ('f' == *p) {
			if (j >= 8) {
				scf_logi("f, task->stack[%d]: %lg\n", i, *(double*)&task->stack_data[i]);
				i++;
			}
			j++;
		}
	}

	scf_logi("-----------\n");
	for (i = 0; i < task->stack_len / sizeof(uintptr_t); i++)
		scf_logi("task->stack[%d]: %#lx\n", i, task->stack_data[i]);

	task->stack_capacity = task->stack_len;

	task->rip = funcptr;

	*ptask = task;
	return 0;
}

static int _co_timer_cmp(scf_rbtree_node_t* node0, void* data)
{
	scf_co_task_t* task0 = (scf_co_task_t*)node0;
	scf_co_task_t* task1 = (scf_co_task_t*)data;

	if (task0->time < task1->time)
		return -1;
	else if (task0->time > task1->time)
		return 1;
	return 0;
}

void scf_co_task_free(scf_co_task_t* task)
{
	if (task->stack_data)
		free(task->stack_data);

	free(task);
}

void scf_co_thread_add_task(scf_co_thread_t* thread, scf_co_task_t* task)
{
	task->time = scf_gettime();

	scf_rbtree_insert(&thread->timers, &task->timer, _co_timer_cmp);

	scf_list_add_tail(&thread->tasks,  &task->list);

	task->thread = thread;

	thread->n_tasks++;
}

int __scf_async(uintptr_t funcptr, const char* fmt, uintptr_t rdx, uintptr_t rcx, uintptr_t r8, uintptr_t r9, uintptr_t* rsp,
		double xmm0, double xmm1, double xmm2, double xmm3, double xmm4, double xmm5, double xmm6, double xmm7)
{
	if (!__co_thread) {
		if (scf_co_thread_open(&__co_thread) < 0) {
			scf_loge("\n");
			return -1;
		}
	}

	scf_co_task_t* task = NULL;

	int ret = scf_co_task_alloc(&task, funcptr, fmt, rdx, rcx, r8, r9, rsp,
			                     xmm0, xmm1, xmm2, xmm3, xmm4, xmm5, xmm6, xmm7);
	if (ret < 0) {
		scf_loge("\n");
		return ret;
	}

	scf_co_thread_add_task(__co_thread, task);
	return 0;
}

int __save_stack(scf_co_task_t* task)
{
	task->stack_len = task->rsp0 - task->rsp;

	scf_logi("task: %p, stack_len: %ld, start: %#lx, end: %#lx, task->rip: %#lx\n",
			task, task->stack_len, task->rsp, task->rsp0, task->rip);

	if (task->stack_len > task->stack_capacity) {
		void* p = realloc(task->stack_data, task->stack_len);
		if (!p) {
			task->err = -ENOMEM;
			return -ENOMEM;
		}

		task->stack_data     = p;
		task->stack_capacity = task->stack_len;
	}

	memcpy(task->stack_data, (void*)task->rsp, task->stack_len);
	return 0;
}

void __async_msleep(int64_t msec)
{
	scf_co_thread_t* thread = __co_thread;
	scf_co_task_t*   task   = thread->current;

	if (task->time > 0)
		scf_rbtree_delete(&thread->timers, &task->timer);

	int64_t time = scf_gettime();
	task->time = time + 1000 * msec;

	scf_rbtree_insert(&thread->timers, &task->timer, _co_timer_cmp);

	scf_loge("task->rip: %#lx, task->rsp: %#lx\n", task->rip, task->rsp);

	__asm_co_task_yield(task, &task->rip, &task->rsp, task->rsp0);
}

static size_t _co_read_from_bufs(scf_co_task_t* task, void* buf, size_t count)
{
	scf_co_buf_t** pp = &task->read_bufs;

	size_t pos = 0;

	while (*pp) {
		scf_co_buf_t* b = *pp;

		size_t len = b->len - b->pos;

		if (len > count - pos)
			len = count - pos;

		memcpy(buf + pos, b->data + b->pos, len);

		pos    += len;
		b->pos += len;

		if (b->pos == b->len) {

			*pp = b->next;

			free(b);
			b = NULL;
		}

		if (pos == count)
			break;
	}

	return pos;
}

static int _co_read_to_bufs(scf_co_task_t* task)
{
	scf_co_buf_t*  b = NULL;

	while (1) {
		if (!b) {
			b = malloc(sizeof(scf_co_buf_t) + 4096);
			if (!b) {
				scf_loge("\n");
				return -ENOMEM;
			}

			b->len = 0;
			b->pos = 0;

			scf_co_buf_t** pp = &task->read_bufs;

			while (*pp)
				pp = &(*pp)->next;
			*pp = b;
		}

		int ret = read(task->fd, b->data + b->len, 4096 - b->len);
		if (ret < 0) {
			if (EINTR == errno)
				continue;
			else if (EAGAIN == errno)
				break;
			else {
				scf_loge("\n");
				return -errno;
			}
		}

		b->len += ret;
		if (4096 == b->len)
			b = NULL;
	}

	return 0;
}

static int _co_add_event(int fd, uint32_t events)
{
	scf_co_thread_t*  thread = __co_thread;
	scf_co_task_t*    task   = thread->current;

	struct epoll_event ev;
	ev.events   = events | EPOLLET | EPOLLRDHUP;
	ev.data.ptr = task;

	if (epoll_ctl(thread->epfd, EPOLL_CTL_ADD, fd, &ev) < 0) {

		if (EEXIST != errno) {
			scf_loge("\n");
			return -errno;
		}

		if (epoll_ctl(thread->epfd, EPOLL_CTL_MOD, fd, &ev) < 0) {
			scf_loge("\n");
			return -errno;
		}
	}

	task->fd     = fd;
	task->events = 0;
	return 0;
}

int __async_connect(int fd, const struct sockaddr *addr, socklen_t addrlen, int64_t msec)
{
	scf_co_thread_t*  thread = __co_thread;
	scf_co_task_t*    task   = thread->current;

	int flags = fcntl(fd, F_GETFL);
	flags |= O_NONBLOCK;
	fcntl(fd, F_SETFL, flags);

	while (1) {
		int ret = connect(fd, addr, addrlen);
		if (ret < 0) {
			if (EINTR == errno)
				continue;
			else if (EINPROGRESS == errno)
				break;
			else {
				scf_loge("\n");
				return -errno;
			}
		} else
			return 0;
	}

	if (task->time > 0)
		scf_rbtree_delete(&thread->timers, &task->timer);

	int64_t time = scf_gettime() + msec * 1000LL;
	task->time   = time;

	scf_rbtree_insert(&thread->timers, &task->timer, _co_timer_cmp);

	int ret = _co_add_event(fd, EPOLLOUT);
	if (ret < 0) {
		scf_loge("\n");
		return ret;
	}

	__asm_co_task_yield(task, &task->rip, &task->rsp, task->rsp0);

	int err = -ETIMEDOUT;

	if (task->events & EPOLLOUT) {

		socklen_t errlen = sizeof(err);

		ret = getsockopt(fd, SOL_SOCKET, SO_ERROR, &err, &errlen);
		if (ret < 0) {
			scf_loge("ret: %d, errno: %d\n", ret, errno);
			err = ret;
		} else {
			err = -err;
		}

		scf_logi("err: %d\n", err);
	}

	if (epoll_ctl(thread->epfd, EPOLL_CTL_DEL, fd, NULL) < 0) {
		scf_loge("EPOLL_CTL_DEL fd: %d, errno: %d\n", fd, errno);
		return -errno;
	}

	if (task->time > 0) {
		scf_rbtree_delete(&thread->timers, &task->timer);
		task->time = 0;
	}

	return err;
}

int __async_read(int fd, void* buf, size_t count, int64_t msec)
{
	scf_co_thread_t*  thread = __co_thread;
	scf_co_task_t*    task   = thread->current;

	int ret = _co_add_event(fd, EPOLLIN);
	if (ret < 0) {
		scf_loge("\n");
		return ret;
	}

	if (task->time > 0)
		scf_rbtree_delete(&thread->timers, &task->timer);

	int64_t time = scf_gettime() + msec * 1000LL;
	task->time   = time;

	scf_rbtree_insert(&thread->timers, &task->timer, _co_timer_cmp);

	size_t pos = 0;

	while (1) {
		int ret = _co_read_to_bufs(task);
		if (ret < 0) {
			scf_loge("\n");
			return ret;
		}

		size_t len = _co_read_from_bufs(task, buf + pos, count - pos);
		pos += len;

		if (pos == count)
			break;

		if (time < scf_gettime())
			break;

		__asm_co_task_yield(task, &task->rip, &task->rsp, task->rsp0);
	}

	if (epoll_ctl(thread->epfd, EPOLL_CTL_DEL, fd, NULL) < 0) {
		scf_loge("EPOLL_CTL_DEL fd: %d, errno: %d\n", fd, errno);
		return -errno;
	}

	return pos;
}

int __async_write(int fd, void* buf, size_t count)
{
	scf_co_thread_t*  thread = __co_thread;
	scf_co_task_t*    task   = thread->current;

	int ret = _co_add_event(fd, EPOLLOUT);
	if (ret < 0) {
		scf_loge("\n");
		return ret;
	}

	size_t pos = 0;

	while (pos < count) {
		int ret = write(task->fd, buf + pos, count - pos);
		if (ret < 0) {
			if (EINTR == errno)
				continue;
			else if (EAGAIN == errno)
				__asm_co_task_yield(task, &task->rip, &task->rsp, task->rsp0);
			else {
				scf_loge("\n");
				return -errno;
			}
		} else
			pos += ret;
	}

	if (epoll_ctl(thread->epfd, EPOLL_CTL_DEL, fd, NULL) < 0) {
		scf_loge("EPOLL_CTL_DEL fd: %d, errno: %d\n", fd, errno);
		return -errno;
	}

	return pos;
}

void __async_exit()
{
	if (__co_thread)
		__co_thread->exit_flag = 1;
}

int __async_loop()
{
	return scf_co_thread_run(__co_thread);
}

#define SCF_CO_TASK_DELETE(task) \
	do { \
		if (task->time > 0) { \
			scf_rbtree_delete(&thread->timers, &task->timer); \
			task->time = 0; \
		} \
		\
		scf_list_del(&task->list); \
		thread->n_tasks--; \
		\
		scf_co_task_free(task); \
		task = NULL; \
	} while (0)

int scf_co_thread_run(scf_co_thread_t* thread)
{
	if (!thread)
		return -EINVAL;

	int n_tasks = thread->n_tasks + 1;

	struct epoll_event* events = malloc(n_tasks * sizeof(struct epoll_event));
	if (!events)
		return -ENOMEM;

	while (!thread->exit_flag) {

		assert(thread->n_tasks >= 0);

		if (0 == thread->n_tasks)
			break;

		if (n_tasks < thread->n_tasks + 1) {
			n_tasks = thread->n_tasks + 1;

			void* p = realloc(events, n_tasks * sizeof(struct epoll_event));
			if (!p) {
				free(events);
				return -ENOMEM;
			}

			events = p;
		}

		int ret = epoll_wait(thread->epfd, events, n_tasks, 10);
		if (ret < 0) {
			scf_loge("errno: %d\n", errno);

			free(events);
			return -1;
		}

		int i;
		for (i = 0; i < ret; i++) {

			scf_co_task_t* task = events[i].data.ptr;
			assert(task);

			task->events = events[i].events;

			int ret2 = __scf_co_task_run(task);

			if (ret2 < 0) {
				scf_loge("ret2: %d, thread: %p, task: %p\n", ret2, thread, task);

				SCF_CO_TASK_DELETE(task);

			} else if (SCF_CO_OK == ret2) {
				scf_logi("ret2: %d, thread: %p, task: %p\n", ret2, thread, task);

				SCF_CO_TASK_DELETE(task);
			}
		}

		while (1) {
			scf_co_task_t* task = (scf_co_task_t*) scf_rbtree_min(&thread->timers, thread->timers.root);
			if (!task)
				break;

			if (task->time > scf_gettime())
				break;

			scf_rbtree_delete(&thread->timers, &task->timer);
			task->time = 0;

			int ret2 = __scf_co_task_run(task);

			if (ret2 < 0) {
				scf_loge("ret2: %d, thread: %p, task: %p\n", ret2, thread, task);

				SCF_CO_TASK_DELETE(task);

			} else if (SCF_CO_OK == ret2) {
				scf_logi("ret2: %d, thread: %p, task: %p\n", ret2, thread, task);

				SCF_CO_TASK_DELETE(task);
			}
		}

	}

	scf_logi("async exit\n");

	free(events);
	return 0;
}
