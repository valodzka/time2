#include <stdlib.h>

#include "postgres.h"
#include "pgtime.h"
#include "pgtz.h"

int main(int argc, char** argv)
{
    {
        char buff[0x100];
        pg_time_t t = 1236899945, t2 = 1247999945;
        pg_tz* tz1 = pg_tzset("Europe/Minsk");
        pg_tz* tz2 = pg_tzset("Japan");

        struct pg_tm tm1, tm11; 
        struct pg_tm tm2, tm22; 

		pg_localtime_r(&t, tz1, &tm1);
		pg_localtime_r(&t, tz2, &tm2);
		pg_localtime_r(&t2, tz1, &tm11);
		pg_localtime_r(&t2, tz2, &tm22);

        printf("Minsk - %d (%s), Japan - %d (%s)\nMinsk - %d (%s), Japan - %d (%s)\n", 
			tm1.tm_hour, tm1.tm_zone, tm2.tm_hour, tm2.tm_zone,
			tm11.tm_hour, tm11.tm_zone, tm22.tm_hour, tm22.tm_zone);

        puts(pg_asctime_r(&tm1,buff));
        puts(pg_asctime_r(&tm2,buff));
    }
    {
     /*   pg_tzenum *tzenum = pg_tzenumerate_start();
        pg_tz* tz = 0;
        while(tz = pg_tzenumerate_next(tzenum)) {
            puts(tz->TZname);
        }
        pg_tzenumerate_end(tzenum);*/
    }
	{
		puts(select_default_timezone()->TZname);
	}
	{
		struct pg_tm tm;
		pg_strptime("17.10.1980", "%d.%m.%Y", &tm);
		printf("%d-%d-%d\n", tm.tm_mday, tm.tm_mon, tm.tm_year);
	}
    
    return (EXIT_SUCCESS);
}

