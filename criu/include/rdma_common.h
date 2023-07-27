#ifndef RDMA_COMMON_H
#define RDMA_COMMON_H

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <getopt.h>
#include <stdbool.h>

#include <netdb.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/socket.h>

#include <rdma/rdma_cma.h>
#include <infiniband/verbs.h>

#define true 1
#define false 0

#define NUM_QUEUES 1

#define CONNECTION_TIMEOUT_MS 2000
#define CQ_CAPACITY 128

#define PAGE_SIZE 4096
#define PAGE_SHIFT 12

struct mr_attr {
	uint64_t addr;
	uint64_t length;
	union stag {
		uint32_t lkey;
		uint32_t rkey;
	} stag;
};

struct device {
	struct ibv_pd *pd;
	struct ibv_context *verbs;
};

struct queue {
	struct ibv_qp *qp;
	struct ibv_cq *cq;
	struct rdma_cm_id *cm_id;
	struct ctrl *ctrl;
};

struct ctrl {
	struct queue *queues;
	struct device *dev;
	struct ibv_comp_channel *comp_channel;
	struct mr_attr clientmr;
	struct mr_attr servermr;
};

int rdma_alloc_session(struct ctrl **session);
int rdma_create_device(struct queue *q);
int rdma_create_queue(struct queue *q, struct ibv_comp_channel *cc);
int rdma_create_mr(struct ibv_pd *pd);

int rdma_poll_cq(struct ibv_cq *cq, int total);
int rdma_recv_wr(struct queue *q, struct mr_attr *sge_mr);
int rdma_send_wr(struct queue *q, enum ibv_wr_opcode opcode,
		struct mr_attr *sge_mr, struct mr_attr *wr_mr);

int start_rdma_server(struct sockaddr_in s_addr);
int start_rdma_client(struct sockaddr_in *s_addr);
int disconnect_rdma_client(void);
#endif
