#!/usr/bin/env python
"""
    check_models_hashes_entry_t generator
    AAS 2009
"""

import hashlib, optparse, os, sys

ALLOWED_EXTENSIONS = ["bsp", "lmp", "md3", "mdl", "spr", "wav"]
VERSION = "1.1"
    
def print_error(n = None):
    """Bad input"""
    if n == 1:
        text = "Directory must exist and be readable"
    elif n == 2:
        text = "Groupname must be alphanumerical value and contain at least one character"
        
    print("Error:%d: %s" % (n, text))
    sys.exit()

def main():
    """Main action"""
    desc = "check_models_hashes_entry_t generator by AAS / 2009"
    usage = "Usage: %prog [options] <directory> <groupname>\n" \
        "Examples: 'c:\>python %prog d:\ ruohis' or '$ ./%prog ~/dirtocache/ unknown'"
    
    parser = optparse.OptionParser(description = desc, usage = usage, version = VERSION)
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
                
                for i in xrange(0, 5):
                    uint = sha1[i*8 : (i+1)*8]
                    uint = [uint[k-2:k] for k in xrange(len(uint), 0, -2)]
                    line += "0x" + "".join(uint)
                    
                    if i < 4:
                        line += ", "
                    
                line += "}, NULL };"
                print(line)
                processed += 1
                
        if opts.verbose:
            print("Files processed: %d" % processed)
            
    sys.exit()

if __name__ == "__main__":
    main()