
#include <stdlib.h>
#include <assert.h>
#include <stdio.h>
#include "cache.h"
using namespace std;

Cache::Cache(int s,int a,int b )
{
   
   ulong i, j;

 lineSize = (ulong)(b);
        sets = 1;               // Only one set for fully associative
        assoc = 10000;        // Large associativity to simulate "infinite" cache
        numLines = assoc;       // The number of lines is equal to associativity
  
   
 
cache = new cacheLine*[sets];
    for (i = 0; i < sets; i++) {
        cache[i] = new cacheLine[assoc];
        for (j = 0; j < assoc; j++) {
            cache[i][j].invalidate();
        }
    }    
   
}

void Cache::MESI_Processor_Access(ulong addr,uchar rw, int copy , Cache **cache, int processor, int num_processors ) {

    cacheLine *block = this->findLine(addr);
    bool isMiss = false;

    if(block == NULL) {
        block = this->fillLine(addr);
        isMiss = true;
        assert(block->getFlags() == INVALID);
    }

    if(isMiss) {
        if(copy)
            Total_execution_time += flush_transfer;
        else
            Total_execution_time += memory_latency;
    }
    
    if(!copy && !(block->isValid())) {
        ++mem_trans;
    }

    if(rw == 'r') {
        ++(this->reads);
        if(isMiss)
            ++(this->readMisses);
        else {
        ++(this->Readhits);
        Total_execution_time += read_hit_latency;
        }

        switch(block->getFlags()) {
            case INVALID: {
                (!copy) ? block->setFlags(Exclusive) : block->setFlags(Shared);
                if(copy) {
                    for(u_int8_t i =0 ; i < num_processors; ++i) {
                        if(i != processor)
                            cache[i]->MESI_Bus_Snoop(addr,1,0,0);
                    }
                }
            } break;

            default: break;
        }

    } else if(rw == 'w') {
        ++(this->writes);
        if(isMiss)
            ++(this->writeMisses);
        else {
        ++(this->Writehits);
        Total_execution_time += write_hit_latency;
        }

        switch(block->getFlags()) {
            case INVALID: {
                for(u_int8_t i =0 ; i < num_processors; ++i) {
                    if(i != processor)
                        cache[i]->MESI_Bus_Snoop(addr,0,1,0);
                }
            }
            break;

            case Shared: {
                for(u_int8_t i =0 ; i < num_processors; ++i) {
                    if(i != processor)
                        cache[i]->MESI_Bus_Snoop(addr,0,0,1);
                }
            }
            break;

            default: break;
        }
        block->setFlags(Modified);
    } else {
        exit(EXIT_FAILURE);
    }
	
}

void Cache::MESI_Bus_Snoop(ulong addr , int busread,int busreadx, int busupgrade ) {
    cacheLine * block = this->findLine(addr);

    if(block == NULL)
        return;

    if(block->getFlags() == Modified && busreadx)
        ++mem_trans;

    if(busreadx || busupgrade) {
        if(!busupgrade)
            ++flushes;
        ++invalidations;
        block->invalidate();
    }
    
    if(busread && (block->getFlags() != INVALID)) {
        ++flushes;
        block->setFlags(Shared);
    }
}

void Cache::MOESI_Processor_Access(ulong addr,uchar rw, int copy, Cache **cache, int processor, int num_processors ) {

    cacheLine *block = this->findLine(addr);
    bool isMiss = false;

    if(block == NULL) {
        block = this->fillLine(addr);
        isMiss = true;
        assert(block->getFlags() == INVALID);
    }

    if(isMiss) {
        if(copy)
            Total_execution_time += flush_transfer;
        else
            Total_execution_time += memory_latency;
    }

    if(!copy && !(block->isValid())) {
        ++mem_trans;
    }

    if(rw == 'r') {
        ++(this->reads);
        if(isMiss)
            ++(this->readMisses);
        else {
        ++(this->Readhits);
        Total_execution_time += read_hit_latency;
        }

        switch(block->getFlags()) {
            case INVALID: {
                (!copy) ? block->setFlags(Exclusive) : block->setFlags(Shared);
                if(copy) {
                    for(u_int8_t i =0 ; i < num_processors; ++i) {
                        if(i != processor)
                            cache[i]->MOESI_Bus_Snoop(addr,1,0,0);
                    }
                }
            } break;

            default: break;
        }

    } else if(rw == 'w') {
        ++(this->writes);
        if(isMiss)
            ++(this->writeMisses);
        else {
        ++(this->Writehits);
        Total_execution_time += write_hit_latency;
        }

        switch(block->getFlags()) {
            case INVALID: {
                for(u_int8_t i =0 ; i < num_processors; ++i) {
                    if(i != processor)
                        cache[i]->MOESI_Bus_Snoop(addr,0,1,0);
                }
                // ++mem_trans;
            }
            break;

            case Shared:
            case Owner: {
                for(u_int8_t i =0 ; i < num_processors; ++i) {
                    if(i != processor)
                        cache[i]->MOESI_Bus_Snoop(addr,0,0,1);
                }
            }
            break;

            default: break;
        }
        block->setFlags(Modified);
    } else {
        exit(EXIT_FAILURE);
    }    

}

void Cache::MOESI_Bus_Snoop(ulong addr , int busread,int busreadx, int busupgrade ) {
	
    cacheLine * block = this->findLine(addr);

    if(block == NULL)
        return;

    switch(block->getFlags()) {
        case Modified:
        case Owner: {

            if(busreadx || busupgrade) {
                if(!busupgrade) {
                    ++flushes;
                    if(block->getFlags() == Owner)
                        ++mem_trans;
                }
                ++invalidations;
                block->invalidate();
            }

            if(busread) {
                ++flushes;
                block->setFlags(Owner);
            }
        } break;

        case Exclusive: {
            if(busreadx) {
                ++invalidations;
                block->invalidate();
            }
            else if(busread)
                block->setFlags(Shared);
        } break;

        case Shared: {
            if(busreadx || busupgrade) {
                ++invalidations;
                block->invalidate();
            }
        } break;

        default: break;
    }
}

/*look up line*/
cacheLine * Cache::findLine(ulong addr)
{
   ulong i, j, tag, pos;
   
   pos = assoc;
   tag = calcTag(addr);
   i   = calcIndex(addr);
  
   for(j=0; j<assoc; j++)
	if(cache[i][j].isValid())
	        if(cache[i][j].getTag() == tag)
		{
		     pos = j; break; 
		}
   if(pos == assoc)
	return NULL;
   else
	return &(cache[i][pos]); 
}


/*Most of these functions are redundant so you can use/change it if you want to*/

/*upgrade LRU line to be MRU line*/
void Cache::updateLRU(cacheLine *line)
{
  line->setSeq(currentCycle);  
}

/*return an invalid line as LRU, if any, otherwise return LRU line*/
cacheLine * Cache::getLRU(ulong addr)
{
   ulong i, j, victim, min;

   victim = assoc;
   min    = currentCycle;
   i      = calcIndex(addr);
   
   for(j=0;j<assoc;j++)
   {
      if(cache[i][j].isValid() == 0) 
	  return &(cache[i][j]);     
   }   
   for(j=0;j<assoc;j++)
   {
	 if(cache[i][j].getSeq() <= min) { victim = j; min = cache[i][j].getSeq();}
   } 
   assert(victim != assoc);
   std::cout << "victim" << victim << std::endl;
   return &(cache[i][victim]);
}

/*find a victim, move it to MRU position*/
cacheLine *Cache::findLineToReplace(ulong addr)
{
   cacheLine * victim = getLRU(addr);
   updateLRU(victim);
  
   return (victim);
}

/*allocate a new line*/
cacheLine *Cache::fillLine(ulong addr)
{ 
   ulong tag;
  
   cacheLine *victim = findLineToReplace(addr);

   assert(victim != 0);
   if ((victim->getFlags() == Modified))
   {
	   writeBack(addr);
   }
   victim->setFlags(INVALID);	
   tag = calcTag(addr);   
   victim->setTag(tag);
       
 

   return victim;
}

void Cache::printStats()
{ 
	//printf("===== Simulation results      =====\n");
	float miss_rate = (float)(getRM() + getWM()) * 100 / (getReads() + getWrites());
	
printf("01. number of reads:                                 %10lu\n", getReads());
printf("02. number of read misses:                           %10lu\n", getRM());
printf("03. number of writes:                                %10lu\n", getWrites());
printf("04. number of write misses:                          %10lu\n", getWM());
printf("05. number of write hits:                            %10lu\n", getWH());
printf("06. number of read hits:                             %10lu\n", getRH()); // Changed from getRM() to getRH()
printf("07. total miss rate:                                 %10.2f%%\n", miss_rate);
printf("08. number of memory accesses (exclude writebacks):  %10lu\n", mem_trans);
printf("09. number of invalidations:                         %10lu\n", Invalidations());
printf("10. number of flushes:                               %10lu\n", Flushes());

	
}

void Cache::printCacheState(ulong state) {
    switch (state) {
        case INVALID:
            std::cout << "I";
            break;
        case Shared:
            std::cout << "S";
            break;
        case Modified:
            std::cout << "M";
            break;
        case Exclusive:
            std::cout << "E";
            break;
        default:
            std::cout << "-";
            break;
    }
}
