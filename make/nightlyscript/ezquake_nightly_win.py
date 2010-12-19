
import datetime
import os
import sys
import subprocess


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
GITBIN		= r'C:\Program Files\Git\bin\git.exe'
DEVENV		= r'C:\Program Files\Microsoft Visual Studio 9.0\Common7\IDE\devenv.exe'
SEVENZIP 	= r'C:\Program Files\7-Zip\7z.exe'
PSCP		= r'C:\Program Files\putty\pscp.exe'
GREPBIN         = r'C:\Program Files\Git\bin\grep.exe'
CUTBIN          = r'C:\Program Files\Git\bin\cut.exe'

# Local paths
TRUNK		= r'C:\ezquake-source'
OUTPUTDIR	= r'%s\make' % (TRUNK, )
SOLUTIONFILE	= r'%s\msvs2008\ezquake_msvs_90.sln' % (OUTPUTDIR, )

# Set this to an empty string if you want to use pageant instead.
KEYLOCATION	= r''

# Server settings
LOGIN		= r''
REMOTEPATH 	= r''
SERVERHOST	= r''

# ---------------------------------------------------------------------------------

def getRevision():
        rev = subprocess.check_output( [GITBIN, 'show'] )
        rev = rev.split('\n')[0].split()[1]
        return rev
        

def main(argv = None):
	# Change dir
	os.chdir(TRUNK)
	
	# Get old revision
        oldrevnum = getRevision()

	# Update from GIT
	print "Updating from GIT..."
	os.system(r'"%s" pull' % GITBIN )
	
	# Get the new revision number
	print "Getting revision information..."
	revnum = getRevision()
	
	print "Old revision: %s" % oldrevnum
	print "New revision: %s" % revnum

        shortrevnum = revnum[:7]
	
	if revnum == oldrevnum:
		print "Same revision as last time, no need to build!!!"
		return 0
	
	# Get the revision log since last time we built
        os.system(r'"%s" whatchanged %s..%s > %s/changes.txt' % (GITBIN, oldrevnum, revnum, OUTPUTDIR))

        # Set BUILD_NUMBER
        os.environ['CL'] = r'/DBUILD_NUMBER#\"%s\"' % shortrevnum

	# Compile ezquake-gl
	print "Compiling ezquake-gl..."
	os.system(r'"%s" %s /rebuild GLRelease /project ezquake /out glLog.txt' % (DEVENV, SOLUTIONFILE))

	# Compile ezquake
	print "Compiling ezquake software..."
	os.system(r'"%s" %s /rebuild Release /project ezquake /out swLog.txt' % (DEVENV, SOLUTIONFILE))

	# Set the names of the compiled files
	softwarename = "ezquake-%s.exe" % shortrevnum
	glname = "ezquake-gl-%s.exe" % shortrevnum
	softwarepdbname = "ezquake-%s.pdb" % shortrevnum
	glpdbname = "ezquake-gl-%s.pdb" % shortrevnum

	# Set name
	zipname = "%s-%s-ezquake.7z" % (datetime.date.today().strftime("%Y-%m-%d"), shortrevnum)
	print zipname

	# Zip it!
	os.chdir(OUTPUTDIR)
	os.system(r'move ezquake.exe %s' % softwarename)
	os.system(r'move ezquake-gl.exe %s' % glname)
	os.system(r'move ezquake.pdb %s' % softwarepdbname)
	os.system(r'move ezquake-gl.pdb %s' % glpdbname)
	os.system(r'"%s" a %s %s %s %s %s README.TXT changes.txt' % (SEVENZIP, zipname, softwarename, glname, softwarepdbname, glpdbname))
	
	# If no key location is supplied, use pageant instead.
	keyuse = ""
	if (KEYLOCATION == ""):
		keyuse = "-pageant"
	else:
		keyuse = "-i %s" % KEYLOCATION
	
	# SCP the file to the server.
	os.system(r'"%s" -scp %s -v -l %s %s\%s %s:%s' % (PSCP, keyuse, LOGIN, OUTPUTDIR, zipname, SERVERHOST, REMOTEPATH))
	
	return 0

if __name__ == "__main__":
	sys.exit(main())

