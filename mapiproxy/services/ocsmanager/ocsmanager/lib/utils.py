"""The applications Utils function"""

from lxml import etree

def validateDocXML(payload):
    """Perform common preliminary XML checks on payload and return
    lxml object on success with error status set to False."""

    try:
        xmlData = etree.XML(payload)
    except etree.XMLSyntaxError:
        return (True, 'Invalid document')
    
    if xmlData.tag is None: return (True, 'Invalid root element')
    if xmlData.tag != 'ocsmanager': return (True, 'Invalid root element %s' % xmlData.tag)

    return (False, xmlData)
