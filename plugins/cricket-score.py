import prof
import urllib2
import json
import time

score_url = "http://api.scorescard.com/?type=score&teamone=Australia&teamtwo=England"

summary = None

# hooks

def prof_init(version, status):
    prof.register_timed(get_scores, 60)

def prof_on_start():
    get_scores()

def get_scores():
    global score_url
    global summary

    notify = None
    new_summary = None
    change = False

    req = urllib2.Request(score_url, None, {'Content-Type': 'application/json'})
    f = urllib2.urlopen(req)
    response = f.read()
    f.close()
    result_json = json.loads(response);

    if 'ms' in result_json.keys():
        new_summary = result_json['ms']
        if new_summary != summary:
            change = True

    if change:
        prof.cons_show("")
        prof.cons_show("Cricket score:")
        if 't1FI' in result_json.keys():
            notify = result_json['t1FI']
            prof.cons_show("  " + result_json['t1FI'])

        if 't2FI' in result_json.keys():
            notify += "\n" + result_json['t2FI']
            prof.cons_show("  " + result_json['t2FI'])

        if 't1SI' in result_json.keys():
            notify += "\n" + result_json['t1SI']
            prof.cons_show("  " + result_json['t1SI'])

        if 't2SI' in result_json.keys():
            notify += "\n" + result_json['t2SI']
            prof.cons_show("  " + result_json['t2SI'])

        summary = new_summary
        notify += "\n\n" + summary
        prof.cons_show("")
        prof.cons_show("  " + summary)
        prof.cons_alert()
        prof.notify(notify, 5000, "Cricket score")
