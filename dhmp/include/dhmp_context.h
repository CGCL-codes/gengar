#ifndef DHMP_CONTEXT_H
#define DHMP_CONTEXT_H

#define DHMP_EPOLL_SIZE 1024

#ifndef ARRAY_SIZE
#define ARRAY_SIZE(arr) (sizeof(arr) / sizeof((arr)[0]))
#endif

typedef void (*dhmp_event_handler)(int fd, void *data_ptr);

struct dhmp_event_data{
	int fd;
	void *data_ptr;
	dhmp_event_handler event_handler;
};


struct dhmp_context{
	int epoll_fd;
	bool stop;
	pthread_t epoll_thread;
};

int dhmp_context_init(struct dhmp_context *ctx);

int dhmp_context_add_event_fd(struct dhmp_context *ctx,
								int events,
								int fd,
								void *data_ptr,
								dhmp_event_handler event_handler);

int dhmp_context_del_event_fd(struct dhmp_context *ctx, int fd);

#endif

