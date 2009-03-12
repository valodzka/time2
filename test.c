#include <stdlib.h>

#include "postgres.h"
#include "pgtime.h"
#include "pgtz.h"

int main(int argc, char** argv)
{
    {
        pg_time_t t = 1236899945;
        pg_tz* tz1 = pg_tzset("Europe/Minsk");
        pg_tz* tz2 = pg_tzset("Japan");

        struct pg_tm tm1; pg_localtime_r(&t, tz1, &tm1);
        struct pg_tm tm2; pg_localtime_r(&t, tz2, &tm2);

        printf("Minsk - %d, Japan - %d\n", tm1.tm_hour, tm2.tm_hour);
    }
    {
        pg_tzenum *tzenum = pg_tzenumerate_start();
        pg_tz* tz = 0;
        while(tz = pg_tzenumerate_next(tzenum)) {
            puts(tz->TZname);
        }
        pg_tzenumerate_end(tzenum);
    }    
    
    return (EXIT_SUCCESS);
}

