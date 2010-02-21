#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>

#include <ieee1284.h>

#define TX_RING_SIZE 65536
#define RX_RING_SIZE 65536
#define TX_CHUNK_SIZE 32
#define RX_CHUNK_SIZE 255
static char txring[TX_RING_SIZE];
static char rxring[RX_RING_SIZE];
static int txi, txj, rxi, rxj;


int evloop (struct parport *port) {
  int count;

 again:
  txj &= TX_RING_SIZE - 1;
  txi &= TX_RING_SIZE - 1;
  rxj &= RX_RING_SIZE - 1;
  rxi &= RX_RING_SIZE - 1;
  if (txj < txi) {
    count = TX_RING_SIZE - txi;
    if (count > TX_CHUNK_SIZE) count = TX_CHUNK_SIZE;
    goto txadvance;
  }
  if (txj > txi) {
    count = txj - txi;
    if (count > TX_CHUNK_SIZE) count = TX_CHUNK_SIZE;
  txadvance:
    count = ieee1284_ecp_write_data(port, F1284_NONBLOCK, 
				    txring + txi, count);
    if (count > 0) {
      txi += count;
    } else if (count < 0) {
      fprintf(stderr,"ECP write failed\n");
      return count;
    }
  }
  if (rxj < rxi) {
    count = RX_RING_SIZE - rxi;
    goto rxadvance;
  }
  if (rxj > rxi) {
    count = rxj - rxi;
  rxadvance:
    if (count > RX_CHUNK_SIZE) count = RX_CHUNK_SIZE;
    count = write(fileno(stdout), rxring + rxi, count);
    if (count > 0) {
      rxi += count;
    } else if (count < 0) {
      fprintf(stderr,"tty write failed\n");
      return count;
    }
  }
  txj &= TX_RING_SIZE - 1;
  txi &= TX_RING_SIZE - 1;
  rxj &= RX_RING_SIZE - 1;
  rxi &= RX_RING_SIZE - 1;
  if (rxi > rxj) {
    count = rxi - rxj;
    goto rxaccrue;
  }
  if (rxi <= rxj) {
    count = RX_RING_SIZE - rxj;
  rxaccrue:
    if (count > RX_CHUNK_SIZE) count = RX_CHUNK_SIZE;
    ieee1284_ecp_fwd_to_rev(port);
    count = ieee1284_ecp_read_data(port, F1284_NONBLOCK, 
				   rxring + rxi, count);
    if (count > 0) {
      rxj += count;
    } else if (count < 0) {
      if (count != E1284_TIMEDOUT) {
	fprintf(stderr,"ECP read failed\n");
	return count;
      }
    }
  }
  /* TODO: select() here */
  usleep(10000);
  if (txi > txj) {
    count = txi - txj;
    goto txaccrue;
  }
  if (txi <= txj) {
    count = TX_RING_SIZE - txj;
  txaccrue:
    if (count > TX_CHUNK_SIZE) count = TX_CHUNK_SIZE;
    count = read(fileno(stdin), txring + txj, count);
    if (count > 0) {
      txj += count;
    } else if (count < 0) {
      if (errno != EAGAIN) {
	perror("tty read failed: ");
	return count;
      }
    }
  }
  goto again;
}



int main ()
{
  struct parport *port;
  struct parport_list pl;
  unsigned int cap;
  int ret;
  struct timeval to;

  ret = fcntl(fileno(stdin),F_GETFL);
  fcntl(fileno(stdin),F_SETFL,ret | O_NONBLOCK);
  ret = fcntl(fileno(stdout),F_GETFL);
  fcntl(fileno(stdout),F_SETFL,O_NONBLOCK);
  ret = 0;

  txi = txj = rxi = rxj = 0;

  ieee1284_find_ports (&pl, 0);
  port = pl.portv[0];
  to.tv_usec = 1000;
  to.tv_sec = 0;
  if (ieee1284_open (port, 0, &cap)) {
    fprintf (stderr, "%s: inaccessible\n", port->name);
    ret = -1;
    goto bail;
  }
  printf ("%s: %#lx", port->name, port->base_addr);
  if (port->hibase_addr)
    printf (" (ECR at %#lx)", port->hibase_addr);
  printf ("\n  ");
  if (ret = ieee1284_claim(port)) {
    fprintf(stderr,"couldn't claim (%i)\n", ret);
    perror("system errno: ");
    goto bail1;
  }
  ieee1284_set_timeout(port, &to);
  ieee1284_terminate(port);
  if (ret = ieee1284_negotiate(port,M1284_ECP)) {
    fprintf(stderr,"couldn't enter ECP mode (%i)\n", ret);
    goto bail2;
  } 
  while (!(ret = evloop(port))) { };
  bail2:
  ieee1284_terminate(port);
  bail1:
  ieee1284_close(port);
  bail:
  return ret;
}


#if 0
1void nonblock(int state)
02{
  03    struct termios ttystate;
  04 
    05    //get the terminal state
    06    tcgetattr(STDIN_FILENO, &ttystate);
  07 
    08    if (state==NB_ENABLE)
    09    {
      10        //turn off canonical mode
	11        ttystate.c_lflag &= ~ICANON;
      12        //minimum of number input read.
	13        ttystate.c_cc[VMIN] = 1;
      14    }
  15    else if (state==NB_DISABLE)
    16    {
      17        //turn on canonical mode
	18        ttystate.c_lflag |= ICANON;
      19    }
  20    //set the terminal attributes.
    21    tcsetattr(STDIN_FILENO, TCSANOW, &ttystate);
  22 
    23}
#endif
