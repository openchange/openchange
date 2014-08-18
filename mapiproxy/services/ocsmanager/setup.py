#!/usr/bin/python
try:
    from setuptools import setup, find_packages
except ImportError:
    from ez_setup import use_setuptools
    use_setuptools()
    from setuptools import setup, find_packages

setup(
    name='ocsmanager',
    version='0.1',
    description='OpenChange Service Manager',
    author='Julien Kerihuel',
    author_email='j.kerihuel@openchange.org',
    url='http://www.openchange.org',
    install_requires=[
        "Pylons>=0.10",
    ],
    setup_requires=["PasteScript>=1.6.3"],
    packages=find_packages(exclude=['ez_setup']),
    include_package_data=True,
    test_suite='nose.collector',
    package_data={'ocsmanager': ['i18n/*/LC_MESSAGES/*.mo']},
    #message_extractors={'ocsmanager': [
    #        ('**.py', 'python', None),
    #        ('templates/**.mako', 'mako', {'input_encoding': 'utf-8'}),
    #        ('public/**', 'ignore', None)]},
    zip_safe=False,
    paster_plugins=['PasteScript', 'Pylons'],
    entry_points="""
    [paste.app_factory]
    main = ocsmanager.config.middleware:make_app

    [paste.app_install]
    main = pylons.util:PylonsInstaller
    """,
)
