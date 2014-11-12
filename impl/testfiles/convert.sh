#!/bin/bash

cat ref_$1.xml | grep trans | wc -l > $1.cuts
cat ref_$1.xml | grep trans | sed "s/<trans type=\"//g" | sed "s/..\" preFNum=\"/ /g" | sed "s/\" postFNum=\"/ /g" | sed "s/\"\/>//g" >> $1.cuts
