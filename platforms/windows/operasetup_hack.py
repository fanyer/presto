import os, sys
path = sys.argv[1]
command = ("python %s..\\..\\modules\\hardcore\\scripts\\operasetup.py -DMSWIN=1 -DSELFTEST=1 --platform_product_config=platforms/windows/config.h --makelang_db=data/strings/english.db --makelang_db=adjunct/quick/english_local.db --makelang_opt=-E --mainline_configuration=current -I%s..\\.." % (path, path)) --generate_pch
print(command)
os.system(command)
