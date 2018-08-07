PARTFILES=\
	  part.BluePillMerge_a126641d7fb79d6ad0f204e9abc523f4_5.fzp \
	  part.DB15F_ae90c0a38d8ea9bd333d1f19c3e22356_7.fzp \
	  svg.breadboard.BluePillMerge_a9966dcd0ea15e21ad555ae4bf4af3e4_1_breadboard.svg \
	  svg.breadboard.DB15F_59ba829fa4b28766056c213d36e42342_1_breadboard.svg \
	  svg.icon.BluePillMerge_a9966dcd0ea15e21ad555ae4bf4af3e4_1_icon.svg \
	  svg.icon.DB15F_59ba829fa4b28766056c213d36e42342_1_icon.svg \
	  svg.pcb.BluePillMerge_a9966dcd0ea15e21ad555ae4bf4af3e4_1_pcb.svg \
	  svg.pcb.DB15F_59ba829fa4b28766056c213d36e42342_1_pcb.svg \
	  svg.schematic.BluePillMerge_a9966dcd0ea15e21ad555ae4bf4af3e4_1_schematic.svg \
	  svg.schematic.DB15F_59ba829fa4b28766056c213d36e42342_1_schematic.svg

.PHONY: all clean prepare

all: usbjoymidi.fzz

clean:
	rm -f usbjoymidi.fzz

%.fzz: %.fz $(PARTFILES)
	zip $@ $?

prepare:
	unzip -o usbjoymidi.fzz
	perl cleanup_pre_commit.pl usbjoymidi.fz > usbjoymidi.fz.new
	mv -f usbjoymidi.fz.new usbjoymidi.fz

