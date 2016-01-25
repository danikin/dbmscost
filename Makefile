dbcalc: dbcalc.o dbcalc_impl.o
	cc -std=c99 dbcalc.o dbcalc_impl.o -o dbcalc

dbcalc_cgi: dbcalc_cgi.o dbcalc_impl.o
	cc -std=c99 dbcalc_cgi.o dbcalc_impl.o -o dbcalc_cgi

dbcalc.o: dbcalc.c dbcalc_impl.h
	cc -std=c99 -c dbcalc.c -o dbcalc.o

dbcalc_impl.o: dbcalc_impl.c dbcalc_impl.h
	cc -std=c99 -c dbcalc_impl.c -o dbcalc_impl.o

dbcalc_cgi.o: dbcalc_cgi.c dbcalc_impl.h
	cc -std=c99 -c dbcalc_cgi.c -o dbcalc_cgi.o

