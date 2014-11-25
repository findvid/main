import FindVid as fv

cuts = fv.getCuts("/home/kanonenfutter/git/findvid/impl/testfiles/hardcuts.mp4")
#cuts = [89, 160, 270, 350, 410, 460, 490, 510, 540, 600, 650, 750]
print (cuts)
print (fv.getFeatures("/home/kanonenfutter/git/findvid/impl/testfiles/hardcuts.mp4", 25, cuts, "/home/kanonenfutter/tmp"))

print (cd.getCutsWrapper("testfiles/hardcuts.mp4"))
