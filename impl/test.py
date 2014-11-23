import FindVid as fv

cuts = fv.getCuts("/home/kanonenfutter/git/findvid/impl/testfiles/hardcuts.mp4")
print (cuts)
print (fv.getFeatures("/home/kanonenfutter/git/findvid/impl/testfiles/hardcuts.mp4", 25, cuts, "/home/kanonenfutter/tmp"))

print (cd.getCutsWrapper("testfiles/hardcuts.mp4"))
