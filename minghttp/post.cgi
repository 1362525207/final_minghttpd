#!/usr/bin/env python
#coding:utf-8
import sys,os
import urllib
length = os.getenv('CONTENT_LENGTH')

if length:
    postdata = sys.stdin.read(int(length))
    print "Content-type:text/html\n"
    print '<html>' 
    print '<head>' 
    print '<title>Result!Thanks!</title>' 
    print '</head>' 
    print '<body bgcolor="#E7E6FA">' 
    print '<center> <h2> Your Post Result </h2> </center>'
    print '<ul>'
    for data in postdata.split('&'):
    	print  '<li>'+data+'</li>'
    print '</ul>'
    print '</body>'
    print '</html>'
    
else:
    print "Content-type:text/html\n"
    print 'no found'


