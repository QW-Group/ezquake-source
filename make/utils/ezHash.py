#!/usr/bin/env python
"""
    ezQuake's check_models_hashes_entry_t generator
    AAS 2009
"""

import getopt, hashlib, optparse, os, sys

ALLOWED_EXTENSIONS = ["bsp", "lmp", "md3", "mdl", "spr", "wav"]
    
def print_error(n = None):
    """Bad input"""
    if n == 1:
        text = "Directory must exists and be readable"
    elif n == 2:
        text = "Groupname must be alphanumerical value [0-9a-zA-Z]"
    elif n == 3:
        text = "No files found to process"
        
    print "Error:%d: %s" % (n, text)
    sys.exit(n)

def main():
    """Main action"""
    
    usage = "Usage: %prog [options] <directory> <groupname>\n" \
        "Examples: 'c:\>python %prog d:\ ruohis' or '$ ./%prog ~/dirtocache/ unknown'"
    
    parser = optparse.OptionParser(usage = usage)
    parser.add_option("-v", "--verbose", help="print additional messages", dest="verbose", default=False, action="store_true")
    (opts, args) = parser.parse_args()

    if len(args) < 2:
        parser.print_help()
    else:
        path = os.path.abspath(os.path.realpath(args[0] + os.path.sep))
        groupname = args[1]
        processed = 0
        
        if not ( os.path.exists(path) and os.path.isdir(path) ):
            print_error(1)
            
        if not groupname.isalnum():
            print_error(2)
            
        for filename in os.listdir(path):
            filename = path + os.path.sep + filename
            name, ext = os.path.basename(filename).rpartition(".")[0::2]
            
            if not os.path.isdir(filename) and ext in ALLOWED_EXTENSIONS:
                f = open(filename, "rb")
                sha1 = hashlib.sha1(f.read()).hexdigest()
                f.close()
                
                line = "static check_models_hashes_entry_t mdlhash_%s_%s = { {" % (groupname.lower(), name.lower())
                
                for i in xrange(0,5):
                    uint = sha1[i*8 : (i+1)*8]
                    uint = [uint[k:k+2] for k in xrange(0, len(uint), 2)]
                    uint.reverse()
                    line += "0x" + "".join(uint)
                    
                    if i != 5:
                        line += ", "
                    
                line += "}, NULL };"
                print line
                processed += 1
                
        if opts.verbose:
            print
            print "Files processed: %d" % processed
            
    sys.exit(0)

if __name__ == "__main__":
    main()