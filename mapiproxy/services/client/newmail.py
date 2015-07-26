#!/usr/bin/env python2.7

import os,sys
import argparse
import ConfigParser
from OCSManager import Network

def parse_config_file(filename):
    """Parse configuration file."""

    cfg = {'username': None, 'password': None, 'encryption': None, 'host': None, 'port': None}
    if filename is None: return cfg

    config = ConfigParser.ConfigParser()
    try:
        config.read(filename)
    except ConfigParser.MissingSectionHeaderError:
        print '[!] Invalid configuration file: %s' % filename
        sys.exit()

    if config.has_section('Config') is False:
        print '[!] Missing Config section'
        sys.exit()

    if config.has_option('Config', 'username'): cfg['username'] = config.get('Config', 'username')
    if config.has_option('Config', 'password'): cfg['password'] = config.get('Config', 'password')
    if config.has_option('Config', 'host'): cfg['host'] = config.get('Config', 'host')
    if config.has_option('Config', 'port'): cfg['port'] = config.getint('Config', 'port')

    # Sanity checks on options
    return cfg

def parse_error(arg):
    print '[!] Missing %s argument' % arg
    sys.exit()

def parse_arguments():
    """Parse command line arguments."""
    parser = argparse.ArgumentParser(description='New Mail notification sender for OCSManager service.',
                                     epilog="Written by Julien Kerihuel <j.kerihuel@openchange.org>")
    parser.add_argument('--config', action='store', help='Path to the newmail configuration file')
    parser.add_argument('--username', action='store', help='Specify the username for the service')
    parser.add_argument('--password', action='store', help='Specify the password for the service')
    parser.add_argument('--host', action='store', help='Specify the host running the service')
    parser.add_argument('--port', action='store', type=int, help='Specify the port where the service is running')
    parser.add_argument('--backend', action='store', required=True, help='Specify the backend that will handle this notification')
    parser.add_argument('--user', action='store', required=True, help='Specify the destination user for this notification')
    parser.add_argument('--folder', action='store', required=True, help='Specify the destination folder for this notification')
    parser.add_argument('--msgid', action='store', required=True, help='Specify the message identifier for this notification')
    parser.add_argument('--verbose', action='store_true', help='Enable verbosity on network transactions')
    args = parser.parse_args()

    cfg = parse_config_file(args.config)

    # Override config parameters if supplied on command line
    if args.username is not None: cfg['username'] = args.username
    if args.password is not None: cfg['password'] = args.password
    if args.host is not None: cfg['host'] = args.host
    if args.port is not None: cfg['port'] = args.port
    cfg['verbose'] = args.verbose

    # Preliminary sanity checks on parameters
    if cfg['username'] is None: parse_error('username')
    if cfg['password'] is None: parse_error('password')
    if cfg['host'] is None: parse_error('host')
    if cfg['port'] is None: parse_error('port')
    
    if cfg['password'].startswith('{SSHA}'):
        cfg['encryption'] = "ssha"
    else:
        print '[I] Assuming plain password'
        cfg['encryption'] = "plain"

    # Retrieve nemail notification parameters
    cfg['newmail'] = {}
    cfg['newmail']['backend'] = args.backend
    cfg['newmail']['username'] = args.user
    cfg['newmail']['folder'] = args.folder
    cfg['newmail']['msgid'] = args.msgid

    return cfg

def error_check(function):
    (error, code) = function
    if error is True:
        print '[!] Error: %s' % code
        sys.exit()

def main():
    cfg = parse_arguments()
    conn = Network.Network(cfg['host'], cfg['port'], cfg['verbose'])

    error_check(conn.login(cfg['username'], cfg['password'], cfg['encryption']))
    print '[I] Authentication successful'

    error_check(conn.newmail(cfg['newmail']))
    print '[I] NewMail notification sent'

if __name__ == "__main__":
    main()
