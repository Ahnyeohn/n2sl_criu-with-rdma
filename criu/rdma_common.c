#include "rdma_common.h"

#ifdef _RDMA_CLIENT_H
const unsigned int MAX_SGE = 1024;
const unsigned int MAX_WR = 1;
extern char client_memory[PAGE_SIZE];
extern struct ibv_mr *client_mr;
#else
const unsigned int MAX_SGE = 64;
const unsigned int MAX_WR = 32;
extern char server_memory[PAGE_SIZE];
extern struct ibv_mr *server_mr;
#endif

int rdma_alloc_session(struct ctrl **session)
{
	int i;

    *session = (struct ctrl *) malloc(sizeof(struct ctrl));
    if (!*session) {
        printf("%s: malloc server_session failed\n", __func__);
        return -EINVAL;
    }
    memset(*session, 0, sizeof(struct ctrl));

    (*session)->queues = (struct queue *) malloc(sizeof(struct queue) * NUM_QUEUES);    
	if (!(*session)->queues) {
        printf("%s: malloc queues failed\n", __func__);
        return -EINVAL;
    }
    memset((*session)->queues, 0, sizeof(struct queue) * NUM_QUEUES);

    for (i = 0; i < NUM_QUEUES; i++) {
        (*session)->queues[i].ctrl = *session;
    }

    return 0;
}

int rdma_create_device(struct queue *q)
{
	struct device *dev;

	dev = (struct device *) malloc(sizeof(*dev));
	if (!dev) {
        printf("%s: malloc dev failed\n", __func__);
        return -EINVAL;
    }

    dev->verbs = q->cm_id->verbs;
    dev->pd = ibv_alloc_pd(dev->verbs);
    if (!dev->pd) {
        printf("%s: ibv_allod_pd failed\n", __func__);
        return -EINVAL;
    }

    q->ctrl->dev = dev;

	return 0;
}

int rdma_create_queue(struct queue *q, struct ibv_comp_channel *cc)
{
    struct ibv_qp_init_attr qp_attr = {};
    int ret;
    
	if (!cc) {
		cc = ibv_create_comp_channel(q->cm_id->verbs);
		if (!cc) {
			printf("%s: ibv_create_comp_channel failed\n", __func__);
			return -EINVAL;
		}
	}

    q->cq = ibv_create_cq(q->cm_id->verbs,
            CQ_CAPACITY,
            NULL,
            cc,
            0);
    if (!q->cq) {
        printf("%s: ibv_create_cq failed\n", __func__);
        return -EINVAL;
    }

    ret = ibv_req_notify_cq(q->cq, 0);
    if (ret) {
        printf("%s: ibv_req_notify_cq failed \n", __func__);
        return ret;
    }

    qp_attr.send_cq = q->cq;
    qp_attr.recv_cq = q->cq;
    qp_attr.qp_type = IBV_QPT_RC;
    qp_attr.cap.max_send_wr = MAX_SGE;
    qp_attr.cap.max_recv_wr = MAX_SGE;
    qp_attr.cap.max_send_sge = MAX_WR;
    qp_attr.cap.max_recv_sge = MAX_WR;

    ret = rdma_create_qp(q->cm_id, q->ctrl->dev->pd, &qp_attr);
    if (ret) {
        printf("%s: rdma_create_qp failed\n", __func__);
        return ret;
    }
    q->qp = q->cm_id->qp;

	return ret;
}

int rdma_create_mr(struct ibv_pd *pd)
{
#ifdef RDMA_CLIENT
	client_mr = ibv_reg_mr(
		pd, 
		client_memory, 
		sizeof(client_memory), 
		IBV_ACCESS_LOCAL_WRITE | 
		IBV_ACCESS_REMOTE_READ | 
		IBV_ACCESS_REMOTE_WRITE);
	if (!client_mr) {
		printf("%s: ibv_reg_mr failed\n", __func__);
		return -EINVAL;
	}
#else
	server_mr = ibv_reg_mr(
		pd, 
		server_memory, 
		sizeof(server_memory), 
		IBV_ACCESS_LOCAL_WRITE | 
		IBV_ACCESS_REMOTE_READ | 
		IBV_ACCESS_REMOTE_WRITE);
	if (!server_mr) {
		printf("%s: ibv_reg_mr failed\n", __func__);
		return -EINVAL;
	}
#endif

	return 0;
}

int rdma_poll_cq(struct ibv_cq *cq, int total) {
    struct ibv_wc wc[total];
    int i, cnt = 0;

    while (cnt < total) {
        cnt += ibv_poll_cq(cq, total, wc);
    }

    if (cnt != total) {
        printf("%s: ibv_poll_cq failed\n", __func__);
        return -EINVAL;
    }

    for (i = 0 ; i < total; i++) {
        if (wc[i].status != IBV_WC_SUCCESS) {
            printf("%s: %s\n", __func__, ibv_wc_status_str(wc[i].status));
            return -EINVAL;
        }
    }

    return 0;
}

int rdma_recv_wr(struct queue *q, struct mr_attr *sge_mr)
{
	struct ibv_recv_wr *bad_wr;
    struct ibv_recv_wr wr;
    struct ibv_sge sge;
    int ret;

	bzero(&sge, sizeof(sge));
	sge.addr = sge_mr->addr;
	sge.length = sge_mr->length;
	sge.lkey = sge_mr->stag.lkey;

	bzero(&wr, sizeof(wr));
	wr.sg_list = &sge;
	wr.num_sge = 1;

    ret = ibv_post_recv(q->qp, &wr, &bad_wr);
    if (ret) {
        printf("%s: ib_post_recv failed\n", __func__);
        return ret;
    }

    return ret;
}

int rdma_send_wr(struct queue *q, enum ibv_wr_opcode opcode, 
		struct mr_attr *sge_mr, struct mr_attr *wr_mr)
{
	struct ibv_send_wr *bad_wr;
	struct ibv_send_wr wr;
	struct ibv_sge sge;
	int ret;

	bzero(&sge, sizeof(sge));
	sge.addr = sge_mr->addr;
	sge.length = sge_mr->length;
	sge.lkey = sge_mr->stag.lkey;

	bzero(&wr, sizeof(wr));
	wr.sg_list = &sge;
	wr.num_sge = 1;
	wr.opcode = opcode;
	wr.send_flags = IBV_SEND_SIGNALED;
	if (wr_mr) {
		wr.wr.rdma.remote_addr = wr_mr->addr;
		wr.wr.rdma.rkey = wr_mr->stag.rkey;
	}

	ret = ibv_post_send(q->qp, &wr, &bad_wr);
	if (ret) {
		printf("%s: ibv_post_send failed\n", __func__);
		return ret;
	}

	return ret;
}
