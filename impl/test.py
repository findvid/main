import FindVid as fv

#cuts = fv.getCuts("testfiles/hardcuts.mp4")
cuts = [0, 89, 179, 269, 299, 329, 359, 389, 573, 603, 633, 693, 737]
print (cuts)
print (fv.getFeatures("testfiles/hardcuts.mp4", 25, cuts, "/tmp"))
