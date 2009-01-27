
import datetime
import os
import sys

# USAGE:
# --------------------------------
# 1. Download putty.exe, pscp.exe, and puttygen.exe from http://www.chiark.greenend.org.uk/~sgtatham/putty/download.html
#
# 2. Run puttygen.exe
#	a) Click "Generate" and follow the instructions
#	b) Enter a Key comment such as "ezquake"
#	c) Enter a passphrase if you like
#	d) Click "Save private key" and put the file in some known location for instance c:\key\ezquake.ppk
#	e) Copy the contents of "Public key for pasting..." textbox into the clipboard.
#
# 3. Run putty.exe
#	a) Login via SSH to the server you want the nightly builds to be uploaded to using the user you want to do the uploading.
#	b) In the homedir:
#		mkdir .ssh
#		cd .ssh
#		edit the file authorized_keys and paste the contents in the clipboard you copied from puttygen.
# 
# USING PAGEANT (optional):
# ----------------------------
# 4. Go to the location where you placed pageant.exe and:
#	a) Create a shortcut to pageant.exe
#	b) Edit the shortcut properties so that the target points to: C:\key\pageant.exe c:\key\ezquake.ppk (Change paths according to your setup)
#	c) Place the shortcut in your Start -> all programs -> startup folder, this way the key will be added to pageant on each startup.
#	d) Run the shortcut a first time so that the key is added to pageant.
# ----------------------------
#
# 5. Set all paths below.
#
# 6. Setup a scheduled task for nightlybuild.cmd to run every night.
#

# Program paths
SVNBIN		= r'C:\Program Files\svn-win32-1.4.4\bin\svn.exe'
DEVENV		= r'C:\Program Files\Microsoft Visual Studio 8\Common7\IDE\devenv.exe'
SEVENZIP 	= r'C:\Program Files\7-Zip\7z.exe'
PSCP		= r'C:\WINDOWS\System32\pscp.exe'

# Local paths
TRUNK		= r'c:\cvs\ezquake_svn_HEAD\\trunk'
OUTPUTDIR	= r'%s\ezquake\make' % (TRUNK, )
SOLUTIONFILE	= r'%s\msvs2005\ezquake_msvs_80.sln' % (OUTPUTDIR, )

# Set this to an empty string if you want to use pageant instead.
KEYLOCATION	= r''

# Server settings
LOGIN		= r'nightly'
REMOTEPATH 	= r'nightlybuilds/win32'
SERVERHOST	= r'uttergrottan.localghost.net'

# ---------------------------------------------------------------------------------

def load_file_as_dict(filename = None):
	if (filename is None):
		return None

	f = open(filename)
	dict = {}

	for line in f:
		if (line.count(":") > 0):
			(key, val) = line.split(":", 1)
			dict[key] = val.strip("\r\n")

	f.close()

	return dict

def main(argv = None):
	# Change dir
	os.chdir(TRUNK)
	
	# Get old revision
	os.system(r'"%s" info > %s/revision.txt' % (SVNBIN, OUTPUTDIR))
	revision = load_file_as_dict("%s/revision.txt" % (OUTPUTDIR, ))
	oldrevnum = revision["Revision"].strip()

	# Update from SVN
	print "Updating from SVN..."
	os.system(r'"%s" update' % (SVNBIN, ))
	
	# Get the new revision number
	print "Getting revision information..."
	os.system(r'"%s" info > %s/revision.txt' % (SVNBIN, OUTPUTDIR))
	revision = load_file_as_dict("%s/revision.txt" % (OUTPUTDIR, ))
	revnum = revision["Revision"].strip()
	
	print "Old revision: %s" % (oldrevnum, )
	print "New revision: %s" % (revnum, )
	
	if int(revnum) == int(oldrevnum):
		print "Same revision as last time, no need to build!!!"
		return 0
	
	# Get the revision log since last time we built
	os.system(r'"%s" log -r %s:%s > %s/changes.txt' % (SVNBIN, int(oldrevnum) + 1, revnum, OUTPUTDIR))

	# Compile ezquake-gl
	print "Compiling ezquake-gl..."
	os.system(r'"%s" %s /rebuild GLRelease /project ezquake' % (DEVENV, SOLUTIONFILE))

	# Compile ezquake
	print "Compiling ezquake software..."
	os.system(r'"%s" %s /rebuild Release /project ezquake' % (DEVENV, SOLUTIONFILE))

	# Set the names of the compiled files
	softwarename = "ezquake-r%s.exe" % revnum
	glname = "ezquake-gl-r%s.exe" % revnum

	# Set name
	zipname = "%s-r%s-ezquake.7z" % (datetime.date.today().strftime("%Y-%m-%d"), revnum)
	print zipname

	# Zip it!
	os.chdir(OUTPUTDIR)
	os.system(r'move ezquake.exe %s' % softwarename)
	os.system(r'move ezquake-gl.exe %s' % glname)
	os.system(r'"%s" a %s %s %s README.TXT revision.txt changes.txt' % (SEVENZIP, zipname, softwarename, glname))
	
	# If no key location is supplied, use pageant instead.
	keyuse = ""
	if (KEYLOCATION == ""):
		keyuse = "-pageant"
	else:
		keyuse = "-i %s" % (KEYLOCATION, )
	
	# SCP the file to the server.
	os.system(r'%s -scp %s -v -l %s %s\%s %s:%s' % (PSCP, keyuse, LOGIN, OUTPUTDIR, zipname, SERVERHOST, REMOTEPATH))
	
	return 0

if __name__ == "__main__":
	sys.exit(main())

