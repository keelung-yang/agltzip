#include <stdio.h>
#include <stdlib.h>
#include <xutility>
#include <string.h>
#include <util/BinFile.h>
#include "jzp.h"
#include "CBitWriter.h"
#include "errors.h"

#define MIN(x, y) (((x)<(y)) ? (x) : (y))

unsigned compress( const void *inbuf, const unsigned inlen, void *outbuf, const unsigned outlen, unsigned dist_width, unsigned size_width, unsigned min_repeat_size)
{
	unsigned dist_max = 1<<dist_width;
	unsigned size_max = (1<<size_width) + min_repeat_size - 1;
	unsigned offset = 0;

  	const unsigned char *in = static_cast<const unsigned char *>(inbuf);
  	
  	CBitWriter *out = new CBitWriter(outbuf, outlen);

  	try 
  	{
  		for(offset=0; offset<MIN(inlen, min_repeat_size); offset++)
  		{
  			out->put(1, 1);
        		out->put(in[offset], 8);
  		}
		
  		while(offset<inlen)
  		{
  			unsigned dict_start = (offset<dist_max) ? 0 : (offset-dist_max);
  			unsigned max_size = 0, max_pos=0;
  			unsigned max_d = offset-dict_start-min_repeat_size;
  			for(unsigned dist=0; dist<max_d; dist++)
  			{
  				const unsigned char *ip = in+offset;
  				const unsigned char *ref = in+dict_start+dist;
  				unsigned max_l = MIN(MIN(size_max, inlen-offset), offset-dict_start-dist);
  				unsigned l;
  				for(l=0; l<max_l; l++)
  				{
  					if (*ref++!=*ip++) break;
  				}
  				if (l>max_size)
  				{
  					max_size = l;
  					max_pos = dist;
  					if (l==size_max) //best case found already, no need to continue
  						break;
  				}
        		}

        		if (max_size>=min_repeat_size)
        		{
        			//printf("repeat %X[%X]->%X\n", dict_start+max_pos, max_size, offset);
        			out->put(0, 1);
        			out->put(max_pos, dist_width);
        			out->put(max_size-min_repeat_size, size_width);
        			offset+=max_size;
        		}
        		else
        		{
        			//printf("put literal %02X from %X\n", in[offset], offset);
        			out->put(1, 1);
        			out->put(in[offset], 8);
        			offset++;
        		}
          	}
          	out->flush();
  	}
        catch (Overflow)
        {
          	puts("Output overflow detected !");
        }
        catch (Misalign)
        {
          	puts("Buffer misalign detected !");
        }
        catch (NegativeDistanse)
        {
          	puts("Negative distanse detected !");
        }
        catch (OutOfBounds)
        {
          	puts("Out of bounds !");
        }
  	
  	if (offset==inlen)
  	{
  		puts("LZSS: OK");
	}
  	unsigned res = out->used();
  	delete out;
  	return res;
}

#define DIST_WIDTH 12
#define SIZE_WIDTH 5
#define MIN_REPEAT 3

void main(int argc, char *argv[])
{
	if (argc!=3)
  	{
    		printf("Usage: %s <in.bin> <out.jzp>\n", argv[0]);
    		return;
  	}

  	unsigned insize;
  	unsigned char *inbuf = LoadBinFile(argv[1], &insize);
  	if (!inbuf)
  	{
    		puts("Can't open input file !");
    		return;
  	}

  	unsigned outsize = insize/8*9+16; //should be enough in worst case (each byte stored as 9-bit "store literal" op)
  	unsigned char *outbuf = new unsigned char[sizeof(JZPHDR)+outsize];

  	memset(outbuf, 0, sizeof(JZPHDR));
  	JZPHDR *hdr = (JZPHDR *)outbuf;
  	hdr->decomp_size = swapl(insize);
  	hdr->decomp_checksum = jzp_checksum(inbuf, insize);
	strncpy(hdr->revision, "@(#)REV: 02.25 Jun  1 2010 08:53:37", 128);    	
	strncpy(hdr->magic, "AGLTZIP", 8);
	hdr->dist_width = DIST_WIDTH;
	hdr->size_width = (SIZE_WIDTH<<4) | MIN_REPEAT;

	unsigned comp_size = compress(inbuf, insize, outbuf+sizeof(JZPHDR), outsize, DIST_WIDTH, SIZE_WIDTH, MIN_REPEAT);
    	if (comp_size==0)
    	{
    		puts("Compression error !");
    		delete outbuf;
    		delete inbuf;
    		return;
	}

  	comp_size+=sizeof(JZPHDR);
  	hdr->comp_size = swapl(comp_size);
  	hdr->comp_checksum = jzp_checksum(outbuf, comp_size);

  	SaveBinFile(argv[2], outbuf, comp_size);
  	
  	delete outbuf;
  	delete inbuf;
}