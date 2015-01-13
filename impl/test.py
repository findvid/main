import FindVid as fv

fv.getFeatures("testfiles/hardcuts.mp4", "0xFUCKU", 50, [25, 50, 75], "/home/kanonenfutter/tmp")

print (fv.getFramerate("testfiles/hardcuts.mp4"))
