/* Simple (hack-ish) stand-alone program to exercise the Vivado AXI Timer */

#include <stdio.h>
#include <inttypes.h>
#include <sys/mman.h>
#include <sys/fcntl.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h> 
#include <getopt.h>
#include <math.h>
#include <time.h>

/* Register defines (word offsets) */
#define TCSR0_WOFF 0
#define TCSR_IRQ   0x100 /* interrupt status */
#define TCSR_ENT   0x080 /* enable timer     */
#define TCSR_ENIT  0x040 /* enable timer irq */
#define TCSR_LOAD  0x020 /* load (and halt)  */
#define TCSR_ARHT  0x010 /* auto-reload      */
#define TCSR_UDT   0x002 /* up (0) / down(1) */

/* Does not include 'LOAD'/'ENT' */
#define TCSR_CONFIG (TCSR_ENIT | TCSR_ARHT | TCSR_UDT)

#define TLR0_WOFF  1
#define TCR0_WOFF  2

typedef volatile uint32_t RegType;

typedef struct TimerType {
	RegType        *bas;
	uint32_t        periodTicks;
	uint32_t        maxlat;
	uint32_t        minlat;
	double          maxper;
	double          minper;
	double          fClk;
	double          period;
	int             fd;
	unsigned        nloops;
	struct timespec lastTime;
	const char     *uiodev;
} TimerType;

static void WR(RegType *r, uint32_t val)
{
	*r = val; // should use proper I/O ASM, probably
}

static uint32_t RD(RegType *r)
{
	return *r; // should use proper I/O ASM, probably
}

static void timer_config(TimerType *t)
{
	WR(t->bas + TLR0_WOFF , t->periodTicks);
	/* clear any pending irq */
	WR(t->bas + TCSR0_WOFF, (TCSR_CONFIG | TCSR_LOAD | TCSR_IRQ));
}

static void timer_start(TimerType *t)
{
	WR(t->bas + TCSR0_WOFF, (TCSR_CONFIG | TCSR_ENT));
}

static uint32_t timer_read(TimerType *t)
{
	return RD(t->bas + TCR0_WOFF);
}

/* Enable UIO interrupts */
static void uio_irq_enable(TimerType *t)
{
uint32_t val;
	val = 1;
	if ( sizeof(val) != write(t->fd, &val, sizeof(val)) ) {
		perror("Writing IRQ fd failed; aborting\n");
		abort();
	}
}

static void usage(const char *nm, TimerType *t)
{
FILE *f = stdout;
	fprintf(f, "Exercise Xilinx AxiTimer (v2.0) on Zynq\n");
	fprintf(f, "usage: %s [-h] [-f <clk_freq_hz>] [-p <irq_period_sec>] [-n <irq_loops>] [-d <uio_dev>]\n\n", nm);
	fprintf(f, "program sets up the AXI timer for periodic interrupt generation and\n");
	fprintf(f, "measures interrupt latency as well as their period.\n");
	fprintf(f, "Uses firmware that instantiates an AxiTimer IP and maps to a UIO device\n");
	fprintf(f, "which also must receive the timer interrupt (device-tree setup).\n");
	fprintf(f, "\n");
	fprintf(f, "    -h                 : this message\n");
	fprintf(f, "    -f <clk_freq_hz>   : firmware AXI clock frequency in Hz (as a double); default: %gHz\n", t->fClk);
	fprintf(f, "    -p <irq_period_sec>: period of interrupt generation in seconds (as a double); default: %gs\n", t->period);
	fprintf(f, "    -n <irq_loops>     : how many interrupts this program should monitor; default: %u\n", t->nloops);
	fprintf(f, "    -d <uid_device>    : which UIO device is mapped to the AxiTimer IP; default: \"%s\"\n", t->uiodev);
	
}

static void irq_handler(TimerType *t)
{
uint32_t        val;
uint32_t        latency;
uint32_t        csr;
struct timespec now;
double          period;
int32_t         secd;

	if ( sizeof(val) != read(t->fd, &val, sizeof(val)) ) {
		perror("Reading IRQ fd failed; aborting\n");
		abort();
	}
	latency = t->periodTicks - timer_read(t);
	/* clear interrupt status */
	csr = RD( t->bas + TCSR0_WOFF );
	csr |= TCSR_IRQ;
	WR(t->bas + TCSR0_WOFF, csr );
	/* re-enable UIO IRQ */
	uio_irq_enable( t );
	if ( latency > t->maxlat ) {
		t->maxlat = latency;
	}
	if ( latency < t->minlat ) {
		t->minlat = latency;
	}

	if ( clock_gettime( CLOCK_MONOTONIC, &now ) ) {
		perror("Unable to read clock; aborting\n");
		abort();
	}

	/* don't compute period for the first iteration */
	if ( t->lastTime.tv_sec > 0 ) {
		secd = 0;
		if ( (period = (double)now.tv_nsec - (double)t->lastTime.tv_nsec) < 0 ) {
			period     += 1.0E9;
			secd       --;
		}
		secd += (int32_t)now.tv_sec - (int32_t)t->lastTime.tv_sec;
		if ( secd ) {
			period += (double)secd * 1.0E9;
		}
		if ( period > t->maxper ) {
			t->maxper = period;
		}
		if ( period < t->minper ) {
			t->minper = period;
		}
	}
	t->lastTime = now;
}

#define MAP_LEN sysconf(_SC_PAGE_SIZE)

int main(int argc, char **argv)
{
int         rval    = 1;
off_t       off     = 0;
unsigned    i;
int         opt;
double     *d_p;
unsigned   *u_p;

TimerType   timer   = {
	bas         : (RegType*)MAP_FAILED,
	fd          : -1,
	fClk        : 100.0E6,
	period      : 1.0E-3,
	minlat      : -1,
	maxlat      : 0,
	minper      : 1.0E20,
	maxper      : 0.0,
	nloops      : 10000,
	uiodev      : "/dev/uio0",
};

	while ( (opt = getopt(argc, argv, "hf:p:n:d:")) > 0 ) {
		d_p = 0;
		u_p = 0;
		switch ( opt ) {
			case 'h':
				rval = 0; /* fall thru */
			default:
				usage( argv[0], &timer );
				return rval;
			case 'f':
				d_p = &timer.fClk;
				break;
			case 'p':
				d_p = &timer.period;
				break;
			case 'n':
				u_p = &timer.nloops;
				break;
			case 'd':
				timer.uiodev = optarg;
				break;
		}
		if ( d_p && 1 != sscanf(optarg, "%lg", d_p) ) {
			fprintf(stderr,"Error: Unable to scan (double) arg to option '-%c'\n", opt);
			return 1;
		}
		if ( u_p && 1 != sscanf(optarg, "%u", u_p) ) {
			fprintf(stderr,"Error: Unable to scan (unsigned) arg to option '-%c'\n", opt);
			return 1;
		}
	}

	if ( (timer.fd = open(timer.uiodev, O_RDWR)) < 0 ) {
		perror("Unable to open timer device");
		goto bail;
	}

	timer.bas = (RegType*)mmap(0, MAP_LEN, PROT_READ | PROT_WRITE, MAP_SHARED, timer.fd, off);

	if ( (RegType*)MAP_FAILED == timer.bas ) {
		perror("Unable to mmap device");
		goto bail;
	}

	timer.periodTicks = (uint32_t)round(timer.fClk * timer.period);

	timer_config( &timer );

	uio_irq_enable( &timer );
	timer_start( &timer );
	for ( i = 0; i < timer.nloops; i++ ) {
		irq_handler( &timer );
	}

	printf("Timer now: %" PRIu32 "\n", timer_read(&timer));
	printf("Max latency: %gus\n", (double)timer.maxlat/timer.fClk*1.E6);
	printf("Min latency: %gus\n", (double)timer.minlat/timer.fClk*1.E6);
	printf("Max period : %gus\n", (double)timer.maxper/           1.E3);
	printf("Min period : %gus\n", (double)timer.minper/           1.E3);
	rval = 0;
bail:
	if ( timer.fd >= 0 ) {
		close( timer.fd );
	}
	if ( (RegType*)MAP_FAILED != timer.bas ) {
		munmap( (void*)timer.bas, MAP_LEN );
	}
	return rval;
}
