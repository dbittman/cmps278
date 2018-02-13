OUTPUTS=$(TEXROOTS:.tex=.pdf)
DEPS=$(TEXROOTS:.tex=.d)

all: $(OUTPUTS)

-include $(DEPS)

%.pdf: %.tex
	latexmk -pdf -M -MF $(DEPS) $<

clean:
	$(RM) *.dvi *.bbl *.blg *.aux *.fls *.log *.fdb_latexmk $(DEPS) *.out *.def *.ques

spotless: clean
	$(RM) $(OUTPUTS)

.PHONY: clean spotless all
