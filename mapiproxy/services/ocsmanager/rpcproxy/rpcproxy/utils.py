import json


def prettify_dict(adict):
    """This method makes use of the JSON API to return a properly indented
    representation of a dict.

    """

    def _unhandled_objects(obj):
        return str(obj)
    lines = json.dumps(adict, default=_unhandled_objects,
                       sort_keys=True, indent=4)
    return "%s\n" % "\n".join([l.rstrip() for l in lines.splitlines()])
