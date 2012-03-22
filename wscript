#!/usr/bin/env python

top = '.'
out = 'build'

APPNAME = 'openchange'
VERSION = None

def options(ctx):
    ctx.load('compiler_c')
    ctx.add_option('--datadir', type='string', default='', dest='datadir', help='read-only application data')

def configure(ctx):
    ctx.define('_GNU_SOURCE', 1)
    ctx.env.append_value('CCDEFINES', '_GNU_SOURCE=1')

    ctx.recurse('libmapi')
    ctx.recurse('libmapi++')
    ctx.recurse('libexchange2ical')
    ctx.recurse('libmapiadmin')
    ctx.recurse('libocpf')
    ctx.recurse('utils')
    ctx.write_config_header('config.h')

def build(ctx):
    ctx.recurse('libmapi')
    ctx.recurse('libmapi++')
    ctx.recurse('libexchange2ical')
    ctx.recurse('libmapiadmin')
    ctx.recurse('libocpf')
    ctx.recurse('utils')
