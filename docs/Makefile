all:	options pictures pdf dvi 

pdf:	gcx.ltx $(PICTURES)
	pdflatex gcx.ltx 

dvi:	gcx.ltx $(EPICTURES)
	latex gcx.ltx 

html:	gcx.ltx 
	latex2html --split=3 --dir html --mkdir gcx.ltx

clean:
	rm *~ ; rm *.aux; rm *.toc; rm *.log

distclean:
	rm *~ ; rm *.aux; rm *.toc; rm *.log; rm *.dvi; rm *.pdf

options:
	../src/gcx --options-doc >options.ltx
	echo '\begin{verbatim}' >cmdline.txt
	../src/gcx --help 2>>cmdline.txt
	echo '\end{verbatim}' >>cmdline.txt
	echo '\begin{verbatim}' >repconv.txt
	../src/gcx --help-rep-conv >>repconv.txt
	echo '\end{verbatim}' >>repconv.txt

pictures:
	./makepictures