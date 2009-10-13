#
#Copyright (C) 2003, 2004, 2005, 2006, 2007, 2009 Olivier Sessink
#All rights reserved.
#
#Redistribution and use in source and binary forms, with or without
#modification, are permitted provided that the following conditions 
#are met:
#  * Redistributions of source code must retain the above copyright 
#    notice, this list of conditions and the following disclaimer.
#  * Redistributions in binary form must reproduce the above 
#    copyright notice, this list of conditions and the following 
#    disclaimer in the documentation and/or other materials provided 
#    with the distribution.
#  * The names of its contributors may not be used to endorse or 
#    promote products derived from this software without specific 
#    prior written permission.
#
#THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS 
#"AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT 
#LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS 
#FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE 
#COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, 
#INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, 
#BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; 
#LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER 
#CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT 
#LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN 
#ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE 
#POSSIBILITY OF SUCH DAMAGE.
#

import os.path
import string
import os
import sys
import stat
import shutil
import glob

def nextpathup(path):
	#if (path[-1:] == '/'):
	#	path = path[:-1]
	try:
		#print 'path='+path
		indx = string.rindex(path,'/')
		if (indx > 0):
			return path[:indx]
	except ValueError:
		pass
	return None

def path_is_safe(path, failquiet=0):
	try:
		statbuf = os.lstat(path)
	except OSError:
		if (failquiet == 0):
			sys.stderr.write('ERROR: cannot lstat() '+path+'\n')
		return -1
	if (not stat.S_ISDIR(statbuf[stat.ST_MODE])):
		if (stat.S_ISLNK(statbuf[stat.ST_MODE])):
			sys.stderr.write('ERROR: '+path+' is a symlink, please point to the real directory\n')
		else:
			sys.stderr.write('ERROR: '+path+' is not a directory!\n')
		return -2
	if (sys.platform[-3:] == 'bsd'):
		# on freebsd root is in group wheel
		if (statbuf[stat.ST_UID] != 0 or statbuf[stat.ST_GID] != grp.getgrnam('wheel').gr_gid):
			sys.stderr.write('ERROR: '+path+' is not owned by root:wheel!\n')
			return -3
	else:
		if (statbuf[stat.ST_UID] != 0 or statbuf[stat.ST_GID] != 0):
			sys.stderr.write('ERROR: '+path+' is not owned by root:root!\n')
			return -3
	if (statbuf[stat.ST_MODE] & stat.S_IWOTH or statbuf[stat.ST_MODE] & stat.S_IWGRP):
		sys.stderr.write('ERROR: '+path+' is writable by group or others!')
		return -4
	return 1

def chroot_is_safe(path, failquiet=0):
	"""tests if path is a safe jail, not writable, no writable /etc/ and /lib, return 1 if all is OK"""
	path = os.path.abspath(path)
	retval = path_is_safe(path,failquiet)
	if (retval < -1):
		return retval
	for subd in 'lib','etc','usr','var','bin','dev','proc','sbin','sys':
		retval = path_is_safe(path+'/'+subd,1)
		if (retval < -1):
			return retval
	npath = nextpathup(path)
	while (npath != None):
		retval = path_is_safe(npath,0)
		if (retval != 1):
#			print 'testing path='+npath+'returned '+str(retval)
			return retval
		npath = nextpathup(npath)
	return 1

def test_suid_sgid(path):
	"""returns 1 if the file is setuid or setgid, returns 0 if it is not"""
	statbuf = os.lstat(path)
	if (statbuf[stat.ST_MODE] & (stat.S_ISUID | stat.S_ISGID)):
		return 1
	return 0

def gen_library_cache(jail):
	if (sys.platform[:5] == 'linux'):
		create_parent_path(jail,'/etc/', 0, copy_permissions=0, allow_suid=0, copy_ownership=0)
		os.system('ldconfig -r '+jail)


def lddlist_libraries_linux(executable):
	"""returns a list of libraries that the executable depends on """
	retval = []
	pd = os.popen3('ldd '+executable)
	line = pd[1].readline()
	while (len(line)>0):
		subl = string.split(line)
		if (len(subl)>0):
			if (subl[0] == 'statically' and subl[1] == 'linked'):
				return retval
			elif (subl[0] == 'not' and subl[2] == 'dynamic' and subl[3] == 'executable'):
				return retval
			elif (subl[0] == 'linux-gate.so.1'):
				pass
			elif (len(subl)==4 and subl[2] == 'not' and subl[3] == 'found'):
				pass
			elif (len(subl)>=3):
				if (os.path.exists(subl[2])):
					retval += [subl[2]]
				else:
					print 'ldd returns non existing library '+subl[2]+' for '+executable
			# on gentoo amd64 the last entry of ldd looks like '/lib64/ld-linux-x86-64.so.2 (0x0000002a95556000)'
			elif (len(subl)>=1 and subl[0][0] == '/'):
				if (os.path.exists(subl[0])):
					retval += [subl[0]]
				else:
					print 'ldd returns non existing library '+subl[0]
			else:
				print 'WARNING: failed to parse ldd output '+line[:-1]
		else:
			print 'WARNING: failed to parse ldd output '+line[:-1]
		line = pd[1].readline()
	return retval

def lddlist_libraries_openbsd(executable):
	"""returns a list of libraries that the executable depends on """
	retval = []
	mode = 3 # openbsd 4 has new ldd output
	pd = os.popen3('ldd '+executable)
	line = pd[1].readline()
	while (len(line)>0):
		subl = string.split(line)
		if (len(subl)>0):
			if (subl[0] == executable+':'):
				pass
			elif (subl[0] == 'Start'):
				if (len(subl)==7 and subl[6] == 'Name'):
					mode = 4
				pass
			elif (len(subl)>=5):
				if (mode == 3):
					if (os.path.exists(subl[4])):
						retval += [subl[4]]
					else:
						print 'ldd returns non existing library '+subl[4]
				elif (mode == 4):
					if (os.path.exists(subl[6])):
						retval += [subl[6]]
					else:
						print 'ldd returns non existing library '+subl[6]
				else:
					print 'unknown mode, please report this bug in jk_lib.py'
			else:
				print 'WARNING: failed to parse ldd output '+line[:-1]
		else:
			print 'WARNING: failed to parse ldd output '+line[:-1]
		line = pd[1].readline()
	return retval

def lddlist_libraries_freebsd(executable):
	"""returns a list of libraries that the executable depends on """
	retval = []
	pd = os.popen3('ldd '+executable)
	line = pd[1].readline()
	while (len(line)>0):
		subl = string.split(line)
		if (len(subl)>0):
			if (len(subl)==1 and subl[0][:len(executable)+1] == executable+':'):
				pass
			elif (len(subl)>=6 and subl[2] == 'not' and subl[4] == 'dynamic'):
				return retval
			elif (len(subl)>=4):
				if (os.path.exists(subl[2])):
					retval += [subl[2]]
				else:
					print 'ldd returns non existing library '+subl[2]
			else:
				print 'WARNING: failed to parse ldd output "'+line[:-1]+'"'
		elif (line[:len(executable)+1] == executable+':'):
			pass
		else:
			print 'WARNING: failed to parse ldd output "'+line[:-1]+'"'
		line = pd[1].readline()
	return retval

def lddlist_libraries(executable):
	if (sys.platform[:5] == 'linux'):
		return lddlist_libraries_linux(executable)
	elif (sys.platform[:7] == 'openbsd'):
		return lddlist_libraries_openbsd(executable)
	elif (sys.platform[:7] == 'freebsd'):
		return lddlist_libraries_freebsd(executable)
	else:
		retval = lddlist_libraries_linux(executable)
		retval += ['/usr/libexec/ld.so','/usr/libexec/ld-elf.so.1','/libexec/ld-elf.so.1']
		return retval

def resolve_realpath(path, chroot=''):
	"""will return the same path that contains not a single symlink directory element"""
	donepath = os.path.basename(path)
	todopath = os.path.dirname(path)
	while (todopath != '/'):
		#print 'todopath=',todopath,'donepath=',donepath
		sb = os.lstat(todopath)
		if (stat.S_ISLNK(sb.st_mode)):
			realpath = os.readlink(todopath)
			if (realpath[0]=='/'):
				todopath = chroot+realpath
				if (todopath[-1:]=='/'):
					todopath = todopath[:-1]
			else:
				todopath = os.path.dirname(todopath)+'/'+realpath
		else:
			donepath = os.path.basename(todopath)+'/'+donepath
			todopath = os.path.dirname(todopath)
		sb=None
	return '/'+donepath

def copy_time_and_permissions(src, dst, be_verbose=0, allow_suid=0, copy_ownership=0):
	# the caller should catch any exceptions!
# similar to shutil.copymode(src, dst) but we do not copy any SUID bits
	sbuf = os.stat(src)
#	in python 2.1 the return value is a tuple, not an object, st_mode is field 0
#	mode = stat.S_IMODE(sbuf.st_mode)
	mode = stat.S_IMODE(sbuf[stat.ST_MODE])
	if (not allow_suid):
		if (mode & (stat.S_ISUID | stat.S_ISGID)):
			print 'removing setuid and setgid permissions from '+dst
#			print 'setuid!!! mode='+str(mode)
			mode = (mode & ~stat.S_ISUID) & ~stat.S_ISGID
#			print 'setuid!!! after mode='+str(mode)
#		print 'mode='+str(mode)
	os.chmod(dst, mode)
	os.utime(dst, (sbuf[stat.ST_ATIME], sbuf[stat.ST_MTIME]))
	if (copy_ownership):
		os.chown(dst, sbuf[stat.ST_UID], sbuf[stat.ST_GID])

def return_existing_base_directory(path):
	"""This function tests if a directory exists, if not tries the parent etc. etc. until it finds a directory that exists"""
	tmp = path
	while (not os.path.exists(tmp) and not (tmp == '/' or tmp=='')):
		tmp = os.path.dirname(tmp)
	return tmp

def create_parent_path(chroot, path, be_verbose=0, copy_permissions=1, allow_suid=0, copy_ownership=0):
	"""creates the directory and all its parents id needed. copy_ownership can only be used if copy permissions is also used"""
	directory = path
	if (directory[-1:] == '/'):
		directory = directory[:-1]
	if (os.path.exists(chroot+directory)):
		return
	tmp = return_existing_base_directory(chroot+directory)
	oldindx = len(tmp)-len(chroot) 
	# find the first slash after the existing directories
	indx = string.find(directory,'/',oldindx+1)
	if (indx == -1 ):
		indx=len(directory)
	while (indx != -1):
		# avoid the /bla//bla pitfall
		if (oldindx +1 == indx):
			oldindx = indx
		else:
			try:
				sb = os.lstat(directory[:indx])
			except OSError, (errno,strerror):
				sys.stderr.write('ERROR: failed to lstat('+directory[:indx]+'):'+strerror+'\n')
				break
			if (stat.S_ISLNK(sb.st_mode)):
				# create the link, create the target, and then continue
				realfile = os.readlink(directory[:indx])
				if (be_verbose):
					print 'Creating symlink '+chroot+directory[:indx]+' to '+realfile
				try:
					os.symlink(realfile, chroot+directory[:indx])
				except OSError, (errno,strerror):
					if (errno == 17): # file exists
						pass
					else:
						sys.stderr.write('ERROR: failed to create symlink '+chroot+directory[:indx]+'\n');
				if (realfile[0]=='/'):
					create_parent_path(chroot, realfile, be_verbose, copy_permissions, allow_suid, copy_ownership)
				else:
					indx2 = string.rfind(directory[:indx],'/')
#					print 'try',directory[:indx2+1]+realfile
					create_parent_path(chroot, directory[:indx2+1]+realfile, be_verbose, copy_permissions, allow_suid, copy_ownership)
			elif (stat.S_ISDIR(sb.st_mode)):
				if (be_verbose):
					print 'Creating directory '+chroot+directory[:indx]
				os.mkdir(chroot+directory[:indx], 0755)
				if (copy_permissions):
					try:
						copy_time_and_permissions(directory[:indx], chroot+directory[:indx], be_verbose, allow_suid, copy_ownership)
					except OSError, (errno,strerror):
						sys.stderr.write('ERROR: failed to copy time/permissions/owner from '+directory[:indx]+' to '+chroot+directory[:indx]+': '+strerror+'\n')
			oldindx = indx
		if (indx==len(directory)):
			indx=-1
		else:
			indx = string.find(directory,'/',oldindx+1)
			if (indx==-1):
				indx=len(directory)


def copy_dir_with_permissions_and_owner(srcdir,dstdir,be_verbose=0):
	# used to **move** home directories into the jail
	#create directory dstdir
	try:
		if (be_verbose):
			print 'Creating directory'+dstdir
		os.mkdir(dstdir)
		copy_time_and_permissions(srcdir, dstdir, be_verbose, allow_suid=0, copy_ownership=1)
	except (IOError, OSError), (errno,strerror):
		sys.stderr.write('ERROR: copying directory and permissions '+srcdir+' to '+dstdir+': '+strerror+'\n')
		return 0
	for root, dirs, files in os.walk(srcdir):
		for name in files:
			if (be_verbose):
				print 'Copying '+root+'/'+name+' to '+dstdir+'/'+name
			try:
				shutil.copyfile(root+'/'+name,dstdir+'/'+name)
				copy_time_and_permissions(root+'/'+name, dstdir+'/'+name, be_verbose, allow_suid=0, copy_ownership=1)
			except (IOError,OSError), (errno,strerror):
				sys.stderr.write('ERROR: copying file and permissions '+root+'/'+name+' to '+dstdir+'/'+name+': '+strerror+'\n')
				return 0
		for name in dirs:
			move_dir_with_permissions_and_owner(root+'/'+name,dstdir+'/'+name,be_verbose)
	return 1

def move_dir_with_permissions_and_owner(srcdir,dstdir,be_verbose=0):
	retval = copy_dir_with_permissions_and_owner(srcdir,dstdir,be_verbose)
	if (retval == 1):
		# remove the source directory
		if (be_verbose==1):
			print 'Removing original home directory '+srcdir
		try:
			shutil.rmtree(srcdir)
		except (OSError,IOError), (errno,strerror):
			sys.stderr.write('ERROR: failed to remove '+srcdir+': '+strerror+'\n')
	else:
		print 'Not everything was copied to '+dstdir+', keeping the old directory '+srcdir

def copy_with_permissions(src, dst, be_verbose=0, try_hardlink=1, retain_owner=0):
	"""copies/links the file and the permissions, except any setuid or setgid bits"""
	do_normal_copy = 1
	if (try_hardlink==1):
		try:
			os.link(src,dst)
			do_normal_copy = 0
		except:
			print 'Linking '+src+' to '+dst+' failed, will revert to copying'
			pass
	if (do_normal_copy == 1):
		try:
			shutil.copyfile(src,dst)
			copy_time_and_permissions(src, dst, be_verbose, allow_suid=0, copy_ownership=retain_owner)
		except (IOError, OSError), (errno,strerror):
			sys.stderr.write('ERROR: copying file and permissions '+src+' to '+dst+': '+strerror+'\n')

def copy_device(chroot, path, be_verbose=1, retain_owner=0):
	# perhaps the calling function should make sure the basedir exists
	create_parent_path(chroot,os.path.dirname(path), be_verbose, copy_permissions=1, allow_suid=0, copy_ownership=0)	
	if (os.path.exists(chroot+path)):
		print 'Device '+chroot+path+' does exist already'
		return
	sb = os.stat(path)
	try:
		if (sys.platform[:5] == 'linux'):
			major = sb.st_rdev / 256 #major = st_rdev divided by 256 (8bit reserved for the minor number)
			minor = sb.st_rdev % 256 #minor = remainder of st_rdev divided by 256
		elif (sys.platform == 'sunos5'):
			if (sys.maxint == 2147483647):
				major = sb.st_rdev / 262144 #major = st_rdev divided by 256 (18 bits reserved for the minor number)
				minor = sb.st_rdev % 262144 #minor = remainder of st_rdev divided by 256
			else:
				#64 bit solaris has 32 bit minor/32bit major
				major = sb.st_rdev / 2147483647
				minor =  sb.st_rdev % 2147483647
		else:
			major = sb.st_rdev / 256 #major = st_rdev divided by 256
			minor = sb.st_rdev % 256 #minor = remainder of st_rdev divided by 256
		if (stat.S_ISCHR(sb.st_mode)): 
			mode = 'c'
		elif (stat.S_ISBLK(sb.st_mode)): 
			mode = 'b'
		else:
			print 'WARNING, '+path+' is not a character or block device'
			return 1
		if (be_verbose==1):
			print 'Creating device '+chroot+path
		ret = os.spawnlp(os.P_WAIT, 'mknod','mknod', chroot+path, str(mode), str(major), str(minor))
		copy_time_and_permissions(path, chroot+path, allow_suid=0, copy_ownership=retain_owner)
	except:
		print 'Failed to create device '+path+', this is a know problem with python 2.1'
		print 'use "ls -l '+path+'" to find out the mode, major and minor for the device'
		print 'use "mknod '+path+' mode major minor" to create the device'
		print 'use chmod and chown to set the permissions as found by ls -l'

def copy_dir_recursive(chroot,dir,force_overwrite=0, be_verbose=0, check_libs=1, try_hardlink=1, retain_owner=0, handledfiles=[]):
	"""copies a directory and the permissions recursive, except any setuid or setgid bits"""
	files2 = ()
	for entry in os.listdir(dir):
		tmp = os.path.join(dir, entry)
		try:
			sbuf = os.lstat(tmp)
			if (stat.S_ISDIR(sbuf.st_mode)):
				create_parent_path(chroot, tmp, be_verbose=be_verbose, copy_permissions=1, allow_suid=0, copy_ownership=retain_owner)			
				handledfiles = copy_dir_recursive(chroot,tmp,force_overwrite, be_verbose, check_libs, try_hardlink, retain_owner, handledfiles)
			else:
				files2 += os.path.join(dir, entry),
		except OSError, e:
			sys.stderr.write('ERROR: failed to investigate source file '+tmp+': '+e.strerror+'\n')
	handledfiles = copy_binaries_and_libs(chroot,files2,force_overwrite, be_verbose, check_libs, try_hardlink, retain_owner, handledfiles)
	return handledfiles 
#	for root, dirs, files in os.walk(dir):
#		files2 = ()
#		for name in files:
#			files2 += os.path.join(root, name),
#		handledfiles = copy_binaries_and_libs(chroot,files2,force_overwrite, be_verbose, check_libs, try_hardlink, retain_owner, handledfiles)
#		for name in dirs:
#			tmp = os.path.join(root, name)
#			create_parent_path(chroot, tmp, be_verbose=be_verbose, copy_permissions=1, allow_suid=0, copy_ownership=retain_owner)			
#			handledfiles = copy_dir_recursive(chroot,os.path.join(root, name),force_overwrite, be_verbose, check_libs, try_hardlink, retain_owner, handledfiles)
#	return handledfiles


# there is a very tricky situation for this function:
# suppose /srv/jail/opt/bin is a symlink to /usr/bin 
# try to lstat(/srv/jail/opt/bin/foo) and you get the result for /usr/bin/foo
# so use resolve_realpath to find you want lstat(/srv/jail/usr/bin/foo)
#
def copy_binaries_and_libs(chroot, binarieslist, force_overwrite=0, be_verbose=0, check_libs=1, try_hardlink=1, retain_owner=0, try_glob_matching=0, handledfiles=[]):
	"""copies a list of executables and their libraries to the chroot"""
	if (chroot[-1] == '/'):
		chroot = chroot[:-1]
	for file in binarieslist:
		if (file in handledfiles):
			continue
		rfile = resolve_realpath(file)
		if (rfile in handledfiles):
			create_parent_path(chroot,os.path.dirname(file), be_verbose, copy_permissions=1, allow_suid=0, copy_ownership=retain_owner)
		 	continue

		try:
			sb = os.lstat(file)
		except OSError, e:
			if (e.errno == 2):
				if (try_glob_matching == 1):
					ret = glob.glob(file)
					if (len(ret)>0):
						handledfiles = copy_binaries_and_libs(chroot, ret, force_overwrite, be_verbose, check_libs, try_hardlink=try_hardlink, retain_owner=retain_owner, try_glob_matching=0, handledfiles=handledfiles)
					elif (be_verbose):
						print 'Source file(s) '+file+' do not exist'
				elif (be_verbose):
					print 'Source file '+file+' does not exist'
			else:
				sys.stderr.write('ERROR: failed to investigate source file '+file+': '+e.strerror+'\n')
			continue
		try:
			chrootsb = os.lstat(chroot+file)
			if (rfile != file):
				chrootsb = os.lstat(chroot+rfile)
			chrootfile_exists = 1
		except OSError, e:
			if (e.errno == 2):
				chrootfile_exists = 0
			else:
				sys.stderr.write('ERROR: failed to investigate destination file '+chroot+file+': '+e.strerror+'\n')
		if ((force_overwrite == 0) and chrootfile_exists and not stat.S_ISDIR(chrootsb.st_mode)):
			if (be_verbose):
				print ''+chroot+file+' already exists, will not touch it'
		else:
			if (chrootfile_exists):
				if (force_overwrite):
					if (stat.S_ISREG(chrootsb.st_mode)):
						if (be_verbose):
							print 'Destination file '+chroot+rfile+' exists, will delete to force update'
						try:
							os.unlink(chroot+rfile)
						except OSError, e:
							sys.stderr.write('ERROR: failed to delete '+chroot+rfile+': '+e.strerror+'\ncannot force update '+chroot+rfile+'\n')
							# BUG: perhaps we can fix the permissions so we can really delete the file?
							# but what permissions cause this error?
					elif (stat.S_ISDIR(chrootsb.st_mode)):
						print 'Destination dir '+chroot+file+' exists'
				else:
					if (stat.S_ISDIR(chrootsb.st_mode)):
						pass
						# for a directory we also should inspect all the contents, so we do not 
						# skip to the next item of the loop
					else:
						if (be_verbose):
							print 'Destination file '+chroot+file+' exists'
						continue
			create_parent_path(chroot,os.path.dirname(file), be_verbose, copy_permissions=1, allow_suid=0, copy_ownership=retain_owner)
			if (stat.S_ISLNK(sb.st_mode)):
				realfile = os.readlink(rfile)
				print 'Creating symlink '+chroot+rfile+' to '+realfile
				try:
					chrootrfile = resolve_realpath(chroot+rfile,chroot)
					os.symlink(realfile, chrootrfile)
				except OSError:
					# if the file exists already
					pass
				handledfiles.append(file)
				if (file != rfile):
					handledfiles.append(rfile)
				if (realfile[0] != '/'):
					realfile = os.path.dirname(rfile)+'/'+realfile
				handledfiles = copy_binaries_and_libs(chroot, [realfile], force_overwrite, be_verbose, check_libs, try_hardlink, retain_owner, handledfiles)
			elif (stat.S_ISDIR(sb.st_mode)):
				handledfiles = copy_dir_recursive(chroot,rfile,force_overwrite, be_verbose, check_libs, try_hardlink, retain_owner, handledfiles)
			elif (stat.S_ISREG(sb.st_mode)):
				if (try_hardlink):
					print 'Trying to link '+rfile+' to '+chroot+rfile
				else:
					print 'Copying '+rfile+' to '+chroot+rfile
				chrootrfile = resolve_realpath(chroot+rfile,chroot)
				copy_with_permissions(rfile,chrootrfile,be_verbose, try_hardlink, retain_owner)
				handledfiles.append(file)
				if (file != rfile):
					handledfiles.append(rfile)
			elif (stat.S_ISCHR(sb.st_mode) or stat.S_ISBLK(sb.st_mode)):
				copy_device(chroot, rfile, be_verbose, retain_owner)
			else:
				sys.stderr.write('Failed to find how to copy '+file+' into a chroot jail, please report to the Jailkit developers\n')
#	in python 2.1 the return value is a tuple, not an object, st_mode is field 0
#	mode = stat.S_IMODE(sbuf.st_mode)
			mode = stat.S_IMODE(sb[stat.ST_MODE])
			if (check_libs and (string.find(rfile, 'lib') != -1 or string.find(rfile,'.so') != -1 or (mode & (stat.S_IXUSR | stat.S_IXGRP | stat.S_IXOTH)))):
				libs = lddlist_libraries(rfile)
				handledfiles = copy_binaries_and_libs(chroot, libs, force_overwrite, be_verbose, 0, try_hardlink, handledfiles)
	return handledfiles

def config_get_option_as_list(cfgparser, sectionname, optionname):
	"""retrieves a comma separated option from the configparser and splits it into a list, returning an empty list if it does not exist"""
	retval = []
	if (cfgparser.has_option(sectionname,optionname)):
		inputstr = cfgparser.get(sectionname,optionname)
		for tmp in string.split(inputstr, ','):
			retval += [string.strip(tmp)]
	return retval

def clean_exit(exitno,message,usagefunc,type='ERROR'):
	print ''
	print type+': '+message
	usagefunc()
	sys.exit(exitno)

def test_numitem_exist(item,num,filename):
	try:
		fd = open(filename,'r')
	except:
		#print ''+filename+' does not exist'
		return 0
	line = fd.readline()
	while (len(line)>0):
		pwstruct = string.split(line,':')
		#print 'len pwstruct='+str(len(pwstruct))+' while looking for '+item
		if (len(pwstruct) > num and pwstruct[num] == item):
			fd.close()
			return 1
		line = fd.readline()
	return 0

def test_user_exist(user, passwdfile):
	return test_numitem_exist(user,0,passwdfile)

def test_group_exist(group, groupfile):
	return test_numitem_exist(group,0,groupfile)

def init_passwd_and_group(chroot,users,groups,be_verbose=0):
	if (chroot[-1] == '/'):
		chroot = chroot[:-1]
	create_parent_path(chroot,'/etc/', be_verbose, copy_permissions=0, allow_suid=0, copy_ownership=0)
	if (sys.platform[4:7] == 'bsd'):
		open(chroot+'/etc/passwd','a').close()
		open(chroot+'/etc/spwd.db','a').close()
		open(chroot+'/etc/pwd.db','a').close()
		open(chroot+'/etc/master.passwd','a').close()
	else:
		if (not os.path.isfile(chroot+'/etc/passwd')):
			fd2 = open(chroot+'/etc/passwd','w')
		else:
	# the chroot passwds file exists, check if any of the users exist already
			fd2 = open(chroot+'/etc/passwd','r+')
			line = fd2.readline()
			while (len(line)>0):
				pwstruct = string.split(line,':')
				if (len(pwstruct) >=3):
					if ((pwstruct[0] in users) or (pwstruct[2] in users)):
						if (be_verbose):
							print 'user '+pwstruct[0]+' exists in '+chroot+'/etc/passwd'
						try:
							users.remove(pwstruct[0])
						except ValueError:
							pass
						try:
							users.remove(pwstruct[2])
						except ValueError:
							pass
				line = fd2.readline()
			fd2.seek(0,2)
		if (len(users) > 0):
			fd = open('/etc/passwd','r')
			line = fd.readline()
			while (len(line)>0):
				pwstruct = string.split(line,':')
				if (len(pwstruct) >=3):
					if ((pwstruct[0] in users) or (pwstruct[2] in users)):
						fd2.write(line)
						if (be_verbose):
							print 'writing user '+pwstruct[0]+' to '+chroot+'/etc/passwd'
						if (not pwstruct[3] in groups):
							groups += [pwstruct[3]]
				line = fd.readline()
			fd.close()
		fd2.close()
	# do the same sequence for the group files
	if (not os.path.isfile(chroot+'/etc/group')):
		fd2 = open(chroot+'/etc/group','w')
	else:
		fd2 = open(chroot+'/etc/group','r+')
		line = fd2.readline()
		while (len(line)>0):
			groupstruct = string.split(line,':')
			if (len(groupstruct) >=2):
				if ((groupstruct[0] in groups) or (groupstruct[2] in groups)):
					if (be_verbose):
						print 'group '+groupstruct[0]+' exists in '+chroot+'/etc/group'
					try:
						groups.remove(groupstruct[0])
					except ValueError:
						pass
					try:
						groups.remove(groupstruct[2])
					except ValueError:
						pass
			line = fd2.readline()
		fd2.seek(0,2)
	if (len(groups) > 0):
		fd = open('/etc/group','r')
		line = fd.readline()
		while (len(line)>0):
			groupstruct = string.split(line,':')
			if (len(groupstruct) >=2):
				if ((groupstruct[0] in groups) or (groupstruct[2] in groups)):
					fd2.write(line)
					if (be_verbose):
						print 'writing group '+groupstruct[0]+' to '+chroot+'/etc/group'
			line = fd.readline()
		fd.close()
	fd2.close()
