# coding=latin-1

import os,sys,re, shutil
import encodings.idna

# Sequence of {name, configuration}

# name = tld
# configuration = {is_tld, not_tld}
# is_tld = array of {name = {is_tld, not_tld}} 
# not_tld = array of names


def add_tld(tld_record, name, not_a_tld):
	#print tld_record
	#print name
	#print "not TLD = ", not_a_tld
	if(name[-1] != "*"):
		domain = encodings.idna.ToASCII(unicode(name[-1], "utf8"))
	else :
		domain = name[-1]
	if len(name) == 1 :
		if not_a_tld :
			tld_record["not-tld"].append(domain)
			#print "========added not TLD ========="
			return
		else:
			if name[0] not in tld_record["is-tld"] :
				tld_record["is-tld"][domain] =  {"is-tld":{}, "not-tld":[]}
			#print "========added TLD ========="
			return
			
	if domain not in tld_record["is-tld"] :
		tld_record["is-tld"][domain] =  {"is-tld":{}, "not-tld":[]}
		
	add_tld(tld_record["is-tld"][domain], name[:-1], not_a_tld)


def handle_us_doms(registry, dom) :
	
	us_geo_domains = [
	"ak.us",
	"al.us",
	"ar.us",
	"as.us",
	"az.us",
	"ca.us",
	"co.us",
	"ct.us",
	"dc.us",
	"de.us",
	"fl.us",
	"ga.us",
	"gu.us",
	"hi.us",
	"ia.us",
	"id.us",
	"il.us",
	"in.us",
	"ks.us",
	"ky.us",
	"la.us",
	"ma.us",
	"md.us",
	"me.us",
	"mi.us",
	"mn.us",
	"mo.us",
	"ms.us",
	"mt.us",
	"nc.us",
	"nd.us",
	"ne.us",
	"nh.us",
	"nj.us",
	"nm.us",
	"nv.us",
	"ny.us",
	"oh.us",
	"ok.us",
	"or.us",
	"pa.us",
	"pr.us",
	"ri.us",
	"sc.us",
	"sd.us",
	"tn.us",
	"tx.us",
	"ut.us",
	"vi.us",
	"vt.us",
	"va.us",
	"wa.us",
	"wi.us",
	"wv.us",
	"wy.us" 
	]
	
	us_geoprefixes = [
	"*",       #  cities, counties, parishes, and townships (locality.state.us)
	"!ci.*",   #  city government agencies (subdomain under locality)
	"!town.*", #  town government agencies (subdomain under locality)
	"!co.*",   #  county government agencies (subdomain under locality)
	"k12",     #  public school districts
	"pvt.k12", #  private schools
	"cc",      #  community colleges
	"tec",     #  technical and vocational schools
	"lib",     #  state, regional, city, and county libraries
	"state",   #  state government agencies
	"gen"      #  general independent entities (groups not fitting into the above categories)
	]
	
	if dom in us_geo_domains :
		for prefix in us_geoprefixes :
			handle_tld_line(registry, prefix + "." + dom, False)
			


def handle_tld_line(registry, line, check_exceptions):

	domainlist = line.split(".")
	
	is_a_tld = True
	
	if domainlist[0].startswith("!") :
		is_a_tld = False
		domainlist[0] = domainlist[0][1:]
		#print "  !  ", domainlist[0]

	tld = encodings.idna.ToASCII(unicode(domainlist[-1], "utf8"))
	
	#print tld
	
	if tld not in registry :
		registry[tld] =  {"is-tld":{}, "not-tld":[]}

	#print "==========="
	
	if len(domainlist) > 1 :
		add_tld(registry[tld], domainlist[:-1], not is_a_tld)

	if check_exceptions :
		#print "checking ", line
		if tld == "us":
			#print "checking ", line
			handle_us_doms(registry, line)
			

def have_wildcard_overrides(sub_domains) :
	if(len(sub_domains["not-tld"]) != 0):
		return True
	if("*" not in sub_domains["is-tld"]):
		return (len(sub_domains["is-tld"]) != 0)
	if(len(sub_domains["is-tld"]) > 1):
		return True
	
	return have_wildcard_overrides(sub_domains["is-tld"]["*"])

			
def count_levels(sub_domains) :
	if("*" not in sub_domains["is-tld"]):
		return 0
	if(len(sub_domains["is-tld"]["*"]["not-tld"]) > 0):
		return 0
	return count_levels(sub_domains["is-tld"]["*"])+1

def output_lists(registry, target_path, body_prefix, license_text, post_process_config):

	if not os.access(target_path, os.F_OK):
		os.makedirs(target_path)
		
	for domain in sorted(registry):
		print domain
		#print registry[domain]
		domain_file_body = license_text
		#domain_file.write('<tld xmlns="http://xmlns.opera.com/tlds" name="'+domain+'"')
		domain_file_body += '<tld name="'+domain+'"'
		levels = 0
		overrides = False
		
		if domain == "name":
			domain_file_body += ' levels="all"'
		else:
			overrides = have_wildcard_overrides(registry[domain])
			levels = count_levels(registry[domain])
			
		if(levels > 0):
			domain_file_body += ' levels="'+str(levels)+'"'
		if not overrides:
			domain_file_body += ' />\n'
		else:		
			domain_file_body += ' >\n'
			domain_file_body += output_list(registry[domain], domain, 1) 
			domain_file_body += '</tld>'
		
		if post_process_config:
			post_process_fun = post_process_config['fun']
			post_process_arg = post_process_config['data']
		else:
			post_process_fun = None
			post_process_arg = None
		
		if callable(post_process_fun):
			full_body = post_process_fun(body_prefix, domain_file_body, post_process_arg)
		else:
			full_body = body_prefix + domain_file_body
		
		domain_file = open(os.path.join(target_path, domain+'.xml'),"wt")
		domain_file.write(full_body)
		domain_file.close()
	
def output_list(sub_registry, domain, level):
	domain_file_body = ""
	if "*" in sub_registry["is-tld"]:
		if have_wildcard_overrides(sub_registry["is-tld"]["*"]):
			print "  "*level, ("*"+'.'+domain), " type all"
			domain_file_body += '  '*level + '<registry all="true" >\n'
			domain_file_body += output_list(sub_registry["is-tld"]["*"], ("*"+'.'+domain), level+1) 
			domain_file_body += '  '*level + '</registry>\n'
		elif level >1:
			print "  "*level, ("*"+'.'+domain), " type registory"
			domain_file_body += '  '*level + '<registry name="'+domain+'"'
			levels = count_levels(sub_registry["is-tld"]["*"])
			if(levels > 0):
				domain_file_body += ' levels="'+str(levels)+'"'
			domain_file_body += ' >\n'
			domain_file_body += output_list(sub_registry["is-tld"]["*"], ("*"+'.'+domain), level+1) 
			domain_file_body += '  '*level + '</registry>\n'

	for sub_domain in sorted(sub_registry["is-tld"]):
		if sub_domain == "*" :
			continue

		print "  "*level, (sub_domain+'.'+domain)
		domain_file_body += '  '*level + '<registry name="'+sub_domain+'"'
		levels = 0
		overrides = have_wildcard_overrides(sub_registry["is-tld"][sub_domain])
		if not overrides:
			levels = count_levels(sub_registry["is-tld"][sub_domain])
		if(levels > 0):
			domain_file_body += ' levels="'+str(levels)+'"'
			
		if not overrides:
			domain_file_body += ' />\n'
		else:		
			domain_file_body += ' >\n'
			domain_file_body += output_list(sub_registry["is-tld"][sub_domain], (sub_domain+'.'+domain), level+1) 
			domain_file_body += '  '*level + '</registry>\n'

	for sub_domain in sorted(sub_registry["not-tld"]):
		print "  "*level, ('!'+sub_domain+'.'+domain)
		domain_file_body += '  '*level + '<domain name="'+sub_domain+'" />\n'

	return domain_file_body


def GenerateFiles(input_file, target_dir, configuration):

	if not input_file or not target_dir or not configuration:
		return
		
	f = open(input_file, "r")

	registry = {}


	for line1 in f:
		line = line1.splitlines()[0].strip()
		if not line:
			continue
		if '\t' in line:
			print "error |"+ line +"|"
		if re.search(r"^//", line) :
			continue

		#print line
		handle_tld_line(registry, line, True)

	print "finished readings\n"

	for x in configuration:
		output_lists(registry, os.path.join(target_dir ,x['version']), x['body_prefix'], 
					x['license'], x['post_processor'])
		shutil.copyfile(input_file, os.path.join(target_dir ,x['version'], os.path.basename(input_file)))

	print "finished\n"
