import os.path
import string
import os
import sys
import stat
import shutil

def nextpathup(path):
	try:
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
			print "ERROR: cannot lstat() "+path+" !"
		return -1
	if (not stat.S_ISDIR(statbuf[stat.ST_MODE])):
		print "ERROR: "+path+" is not a directory!"
		return -2
	if (statbuf[stat.ST_UID] != 0 or statbuf[stat.ST_GID] != 0):
		print "ERROR: "+path+" is not owned by root:root!"
		return -3
	if (statbuf[stat.ST_MODE] & stat.S_IWOTH or statbuf[stat.ST_MODE] & stat.S_IWGRP):
		print "ERROR: "+path+" is writable by group or others!"
		return -4	

def chroot_is_safe(path, failquiet=0):
	"""tests if path is a safe jail, not writable, no writable /etc/ and /lib, return 1 if all is OK"""
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
			return retval
		npath = nextpathup(path)

def test_suid_sgid(path):
	"""returns 1 if the file is setuid or setgid, returns 0 if it is not"""
	statbuf = os.lstat(path)
	if (statbuf[stat.ST_MODE] & (stat.S_ISUID | stat.S_ISGID)):
		return 1
	return 0

def lddlist_libraries(executable):
	"""returns a list of libraries that the executable depends on """
	retval = []
	pd = os.popen3('ldd '+executable)
	line = pd[1].readline()
	while (len(line)>0):
		subl = string.split(line)
		if (len(subl)>0):
			if (subl[0] == 'statically' and subl[1] == 'linked'):
				return retval
			if (subl[0] == 'linux-gate.so.1'):
				pass
			if (len(subl)>=3):
				if (os.path.exists(subl[2])):
					retval += [subl[2]]
				else:
					print 'ldd returns not existing library '+subl[2]
		line = pd[1].readline()
	return retval

def create_full_path(directory, be_verbose=0):
	"""creates the directory and all its parents id needed"""
#	print 'create_full_path, started for directory '+directory
	if (directory[-1:] == '/'):
		directory = directory[:-1]
	if (os.path.exists(directory)):
		return
	tmp = directory
	while (not os.path.exists(tmp)):
#		print tmp+' does not exist'
		tmp = os.path.dirname(tmp)
	indx = string.find(directory,'/',len(tmp)+1)
	while (indx != -1):
		if (be_verbose):
			print 'creating directory '+directory[:indx]
		os.mkdir(directory[:indx])
		indx = string.find(directory,'/',indx+1)
	os.mkdir(directory)

def copy_permissions(src, dst, be_verbose=0, allow_suid=0):
	sbuf = os.stat(src)
#	in python 2.1 the return value is a tuple, not an object, st_mode is field 0
#	mode = stat.S_IMODE(sbuf.st_mode)
	mode = stat.S_IMODE(sbuf[stat.ST_MODE])
	if (not allow_suid):
		if (mode & (stat.S_ISUID | stat.S_ISGID)):
			if (be_verbose):
				print 'removing setuid and setgid permissions from '+dst
#			print 'setuid!!! mode='+str(mode)
			mode = (mode & ~stat.S_ISUID) & ~stat.S_ISGID
#			print 'setuid!!! after mode='+str(mode)
#		print 'mode='+str(mode)
	os.chmod(dst, mode)

def copy_with_permissions(src, dst, be_verbose=0):
	"""copies the file and the permissions, except any setuid or setgid bits"""
	shutil.copyfile(src,dst)
	copy_permissions(src, dst, be_verbose, 0)

def copy_binaries_and_libs(chroot, binarieslist, force_overwrite=0, be_verbose=0, check_libs=1):
	"""copies a list of executables and their libraries to the chroot"""
	if (chroot[-1] == '/'):
		chroot = chroot[:-1]
	for file in binarieslist:
		if (not os.path.exists(file)):
			if (be_verbose):
				print 'source file '+file+' does not exist'
			break
		if ((force_overwrite == 0) and os.path.isfile(chroot+file)):
			if (be_verbose):
				print ''+chroot+file+' exists'
		else:
			create_full_path(chroot+os.path.dirname(file),be_verbose)
			if (os.path.islink(file)):
				realfile = os.readlink(file)
				if (be_verbose):
					print 'creating symlink '+chroot+file+' to '+realfile
				os.symlink(realfile, chroot+file)
				if (realfile[1] != '/'):
					realfile = os.path.dirname(file)+'/'+realfile
				copy_binaries_and_libs(chroot, [realfile], force_overwrite, be_verbose, 1)
			else:
				if (be_verbose):
					print 'copying '+file+' to '+chroot+file
				copy_with_permissions(file,chroot+file,be_verbose)
			if (check_libs):
				libs = lddlist_libraries(file)
				copy_binaries_and_libs(chroot, libs, force_overwrite, be_verbose, 0)

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

def test_firstitem_exist(item, filename):
	fd = open(filename,'r+')
	line = fd.readline()
	while (len(line)>0):
		pwstruct = string.split(line,':')
		if (pwstruct[0] == item):
			fd.close()
			return 1
		line = fd.readline()
	return 0

def test_user_exist(user, passwdfile):
	return test_firstitem_exist(user,passwdfile)

def test_group_exist(group, groupfile):
	return test_firstitem_exist(group,groupfile)

def init_passwd_and_group(chroot,users,groups,be_verbose=0):
	if (chroot[-1] == '/'):
		chroot = chroot[:-1]
	create_full_path(chroot+'/etc/', be_verbose)
	if (not os.path.isfile(chroot+'/etc/passwd')):
		fd2 = open(chroot+'/etc/passwd','w')
	else:
# the chroot passwds file exists, check if any of the users exist already
		fd2 = open(chroot+'/etc/passwd','r+')
		line = fd2.readline()
		while (len(line)>0):
			pwstruct = string.split(line,':')
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
			if ((groupstruct[0] in groups) or (groupstruct[2] in groups)):
				fd2.write(line)
				if (be_verbose):
					print 'writing group '+groupstruct[0]+' to '+chroot+'/etc/group'
			line = fd.readline()
		fd.close()
	fd2.close()
