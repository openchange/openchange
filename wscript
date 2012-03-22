#!/usr/bin/env python

top = '.'
out = 'build'

APPNAME = 'openchange'
VERSION = None

def options(ctx):
    ctx.load('compiler_c')
    ctx.add_option('--datadir', type='string', default='', dest='datadir', help='Read-only application data')
    ctx.add_option('--with-modulesdir', type='string', 
                   default='', dest='modulesdir', help='Modules path to use')

def configure(ctx):
    ctx.define('_GNU_SOURCE', 1)
    ctx.env.append_value('CCDEFINES', '_GNU_SOURCE=1')

    ctx.recurse('libmapi')
    ctx.recurse('libmapi++')
    ctx.recurse('libexchange2ical')
    ctx.recurse('libmapiadmin')
    ctx.recurse('libocpf')
    ctx.recurse('utils')
    ctx.recurse('mapiproxy')
    ctx.write_config_header('config.h')

def build(ctx):
    ctx.recurse('libmapi')
    ctx.recurse('libmapi++')
    ctx.recurse('libexchange2ical')
    ctx.recurse('libmapiadmin')
    ctx.recurse('libocpf')
    ctx.recurse('utils')
    ctx.recurse('mapiproxy')
