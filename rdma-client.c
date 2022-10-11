#include "rdma-common.h"

#define PAGE_NUM 4
#define SHOW_INTERVAL 3
#define SCAN_INTERVAL 1
#define RDMA_BLOCK_SIZE 32
const int TIMEOUT_IN_MS = 500; /* ms */


static char* mapped_mem;
static int pagesize;

static int on_addr_resolved(struct rdma_cm_id *id);
static int on_connection(struct rdma_cm_id *id);
// static int on_disconnect(struct rdma_cm_id *id);
// static int on_event(struct rdma_cm_event *event);
static int on_route_resolved(struct rdma_cm_id *id);
static void * show_buffer_client();
static void * check_and_rdmasend();
static void usage(const char *argv0);

static int written_flag = 0;
static int written_page_id[PAGE_NUM];

u_int64_t get_page_index(void * addr) {
  u_int64_t lack_addr = (u_int64_t)addr - (u_int64_t)mapped_mem;
  return lack_addr / pagesize;
}

void set_page_written(uint64_t page_index) {
  written_flag = 1;
  written_page_id[page_index] = 1;
  printf("set written_flag to 1\nset page %ld to 1\n", page_index);
}

void segv_handler(int sig, siginfo_t *si, void *unused) {
  printf("got the error!\n");
  u_int64_t page_index = get_page_index(si->si_addr);
  set_page_written(page_index);
  printf("addr:0x%lx\nerror page:%ld\n", (long)si->si_addr, (long)page_index);
  mprotect(mapped_mem + page_index*pagesize, pagesize, PROT_READ | PROT_WRITE);
}


int main(int argc, char **argv)
{
  struct addrinfo *addr;
  struct rdma_cm_event *event = NULL;
  struct rdma_cm_id *conn= NULL;
  struct rdma_event_channel *ec = NULL;
  pagesize = sysconf(_SC_PAGE_SIZE);
  mapped_mem = memalign(pagesize, PAGE_NUM*pagesize);

  printf("page size is:%d\n", pagesize);

  struct sigaction sa;
  memset(&sa, 0, sizeof(sa));
  int sa_flag = SA_SIGINFO;
  sa.sa_sigaction = &segv_handler;
  sa.sa_flags = sa_flag;
  sigaction(SIGSEGV, &sa, NULL);

  set_client(1);

  if (argc != 3)
    usage(argv[0]);

  if (strcmp(argv[1], "write") == 0)
    set_mode(M_WRITE);
  else if (strcmp(argv[1], "read") == 0)
    set_mode(M_READ);
  else
    usage(argv[0]);

  TEST_NZ(getaddrinfo(argv[2], DEFAULT_PORT, NULL, &addr));

  TEST_Z(ec = rdma_create_event_channel());
  TEST_NZ(rdma_create_id(ec, &conn, NULL, RDMA_PS_TCP));
  TEST_NZ(rdma_resolve_addr(conn, NULL, addr->ai_addr, TIMEOUT_IN_MS));

  freeaddrinfo(addr);

  struct rdma_cm_event event_copy;
  int r = 0;
  rdma_get_cm_event(ec, &event);
  memcpy(&event_copy, event, sizeof(*event));
  rdma_ack_cm_event(event);
  if ((&event_copy)->event == RDMA_CM_EVENT_ADDR_RESOLVED) {
    r = on_addr_resolved((&event_copy)->id);
    if (r == 0);
  }

  rdma_get_cm_event(ec, &event);
  memcpy(&event_copy, event, sizeof(*event));
  rdma_ack_cm_event(event);
  if ((&event_copy)->event == RDMA_CM_EVENT_ROUTE_RESOLVED) {
    r = on_route_resolved((&event_copy)->id);
  }

  rdma_get_cm_event(ec, &event);
  memcpy(&event_copy, event, sizeof(*event));
  rdma_ack_cm_event(event);
  if ((&event_copy)->event == RDMA_CM_EVENT_ESTABLISHED) {
    r = on_connection((&event_copy)->id);
  }
  printf("on cennection.\n");

  
  TEST_NZ(mprotect(mapped_mem, pagesize*PAGE_NUM, PROT_READ));

  pthread_t memory_monitor;
    TEST_NZ(pthread_create(&memory_monitor, NULL, show_buffer_client, NULL));

  pthread_t scanner;
    TEST_NZ(pthread_create(&scanner, NULL, check_and_rdmasend, NULL));
  sleep(5);

  printf("addr:0x%lx\n", (u_int64_t)mapped_mem);
  *mapped_mem = 'a';
  sleep(3);
  *mapped_mem = 'b';
  
  sleep(10);
  *(mapped_mem+1) = 'c';
  *(mapped_mem+pagesize) = 'f';
  sleep(2);
  *(mapped_mem+2*pagesize+1) = 't';
  sleep(3);
  *(mapped_mem+2*pagesize) = 's';
  
  
  // *(mapped_mem + pagesize) = 'b';
  while (1)
  {
    
  };
  

  rdma_destroy_event_channel(ec);

  return 0;
}

int on_addr_resolved(struct rdma_cm_id *id)
{
  printf("address resolved.\n");

  build_connection(id);
  // sprintf(get_local_message_region(id->context), "message from active/client side with pid %d", getpid());
  TEST_NZ(rdma_resolve_route(id, TIMEOUT_IN_MS));

  return 0;
}

int on_connection(struct rdma_cm_id *id)
{
  on_connect(id->context);
  send_mr(id->context);

  return 0;
}

// int on_disconnect(struct rdma_cm_id *id)
// {
//   printf("disconnected.\n");

//   destroy_connection(id->context);
//   return 1; /* exit event loop */
// }

// int on_event(struct rdma_cm_event *event)
// {
//   int r = 0;

//   if (event->event == RDMA_CM_EVENT_ADDR_RESOLVED)
//     r = on_addr_resolved(event->id);
//   else if (event->event == RDMA_CM_EVENT_ROUTE_RESOLVED)
//     r = on_route_resolved(event->id);
//   else if (event->event == RDMA_CM_EVENT_ESTABLISHED)
//     r = on_connection(event->id);
//   else if (event->event == RDMA_CM_EVENT_DISCONNECTED)
//     r = on_disconnect(event->id);
//   else {
//     fprintf(stderr, "on_event: %d\n", event->event);
//     die("on_event: unknown event.");
//   }

//   return r;
// }

int on_route_resolved(struct rdma_cm_id *id)
{
  struct rdma_conn_param cm_params;

  printf("route resolved.\n");
  build_params(&cm_params);
  TEST_NZ(rdma_connect(id, &cm_params));

  return 0;
}

void usage(const char *argv0)
{
  fprintf(stderr, "usage: %s <mode> <server-address>\n  mode = \"read\", \"write\"\n", argv0);
  exit(1);
}


void * show_buffer_client() {
  while(1) {
    sleep(SHOW_INTERVAL);
    for (int i = 0; i < 4; i++) {
      printf("PAGE %d:%s\n", i, mapped_mem+i*pagesize);
    }
    puts("");
  }
  return NULL;
  
}

void * check_and_rdmasend() {
  char* send_word = malloc(pagesize);
  while (1)
  {
    sleep(SCAN_INTERVAL);
    
    if (written_flag == 0) continue;
    
    for (int i = 0; i < PAGE_NUM; i++) {
      if (written_page_id[i] == 0) continue;
      TEST_NZ(mprotect(mapped_mem+pagesize*i, pagesize, PROT_READ));
      memset(send_word, 0, pagesize);
      written_page_id[i] = 0;
      memcpy(send_word, mapped_mem+i*pagesize, pagesize);
      printf("RDMA WRITE: %s\n", send_word);
      do_rdma_send(send_word, i);
    }
  }
  return NULL;
}