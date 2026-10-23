# Vendored from Joseph1010805/HeavenClient-Android (tools/gen_quest_scripts.py @ c5a077a), AGPL-3.0.
"""Give every quest its dialogue back, out of the game's own data.

Cosmic ships 708 NPC scripts and FOUR of them are below id 3000, so Maple
Island and Lith Harbor - the first two hours of the game - are silent. Every
quest giver there answers with a log line: "NPC Pio (10000) is not coded".

The words were never missing. `Quest.nx/Say.img` holds the full conversation
for 2800 quests: what the giver says, the accept and decline replies, what
they say when you hand it in, and what they say when you have not finished.
Nothing reads it - Cosmic has no reference to Say.img anywhere.

This writes one `scripts/quest/<id>.js` per quest that has dialogue and does
not already have a hand-written script, in the shape Cosmic's quest scripts
already take. That means no Java conversation engine to write and no new
packets: the generated scripts ride the same machinery the 253 existing ones
do, which is machinery already known to work.

    python tools/gen_quest_scripts.py <Quest.nx> <cosmic>/scripts/quest [--force]

NEVER overwrites an existing script. The hand-written ones do things no
generator can - Roger's quest hurts you and hands you an apple - and a
generated file that only says the words would be a downgrade.
"""
import io
import os
import struct
import sys

HEADER = struct.Struct('<I I Q I Q I Q I Q')
NODE = struct.Struct('<I I H H 8s')


class Nx:
    def __init__(self, path):
        with open(path, 'rb') as f:
            self.data = memoryview(f.read())

        (_, self.node_count, self.node_off,
         self.string_count, self.string_off,
         self.bitmap_count, self.bitmap_off,
         self.audio_count, self.audio_off) = HEADER.unpack_from(self.data, 0)

    def node(self, i):
        return NODE.unpack_from(self.data, self.node_off + i * NODE.size)

    def name(self, i):
        return self.string(self.node(i)[0])

    def string(self, i):
        off = struct.unpack_from('<Q', self.data, self.string_off + i * 8)[0]
        n = struct.unpack_from('<H', self.data, off)[0]
        return bytes(self.data[off + 2:off + 2 + n]).decode('utf-8', 'replace')

    def children(self, idx):
        _, first, num, _, _ = self.node(idx)
        return [(self.name(i), i) for i in range(first, first + num)]

    def child(self, idx, want):
        for n, i in self.children(idx):
            if n == want:
                return i
        return None

    def resolve(self, path):
        i = 0
        for part in [p for p in path.split('/') if p]:
            i = self.child(i, part)
            if i is None:
                return None
        return i

    def text(self, i):
        _, _, _, t, payload = self.node(i)

        if t == 3:
            return self.string(struct.unpack_from('<I', payload)[0])

        return None

    def number(self, i):
        """An integer node, or an integer written as a string.

        Say.img is inconsistent about this - `answer` and `ask` are ints in
        some quests and strings in others, exactly like `life/id` in Map.nx.
        Reading one as the other yields None or 0 and the quiz silently is
        not a quiz, which is how the first attempt at this generated 2,392
        scripts and nought quizzes.
        """
        if i is None:
            return None

        _, _, _, t, payload = self.node(i)

        if t == 1:
            return struct.unpack_from('<q', payload)[0]

        if t == 3:
            try:
                return int(self.string(struct.unpack_from('<I', payload)[0]))
            except ValueError:
                return None

        return None


def lines(nx, idx):
    """The numbered messages directly under a node, in order.

    Say.img numbers them "0", "1", "2" as CHILD NAMES, not as an array, and
    a node can carry `yes`/`no`/`stop` beside them - so this takes only the
    children whose names are digits and sorts them numerically. Sorting them
    as strings puts "10" before "2", which reorders a conversation.
    """
    if idx is None:
        return []

    out = []

    for name, i in nx.children(idx):
        if not name.isdigit():
            continue

        said = nx.text(i)

        if said:
            out.append((int(name), said))

    out.sort(key=lambda pair: pair[0])

    return [said for _, said in out]


def quiz(nx, phase):
    """The questions in a phase, if it is a quiz. Empty if it is not.

    A phase marked `ask = 1` is not a conversation, it is an examination, and
    Say.img spells the whole thing out:

        1:
          0,1,2,3       the questions - each one OPENS with the praise for
                        having got the previous one right, so they are also
                        the "correct" reply
          ask = 1       this phase is a quiz
          stop:
            0: { answer = 2, 0 = "Incorrect! ...", ... }
            1: { answer = 2, 0 = ..., 2 = ..., 3 = ..., 4 = ... }

    `answer` is the correct option counted from ONE - question 0's options are
    "#L0 Yes / #L1 No" and its answer is 2, which is "No". Everything else in
    `stop/<n>` is keyed by the option chosen, counted from zero, and is what
    the NPC says when you pick it.

    The generator used to ignore all of this and emit `sendNext` for a page
    full of "#L0# ... #l" options - so every answer was accepted, the reply
    always congratulated you, and the quiz could not be failed or, really,
    taken. 495 of the 2,392 generated scripts had a question in them.
    """
    if phase is None:
        return []

    marker = nx.child(phase, 'ask')

    if marker is None:
        return []

    stop = nx.child(phase, 'stop')

    if stop is None:
        return []

    asked = lines(nx, phase)
    out = []

    for name, node in sorted(nx.children(stop),
                             key=lambda pair: int(pair[0]) if pair[0].isdigit() else -1):
        if not name.isdigit():
            continue

        which = int(name)

        if which >= len(asked):
            break

        answer = nx.child(node, 'answer')

        if answer is None:
            continue

        correct = nx.number(answer)

        if correct is None:
            continue

        correct -= 1

        wrong = {}

        for opt, i in nx.children(node):
            if opt.isdigit():
                said = nx.text(i)

                if said:
                    wrong[int(opt)] = said

        out.append({'ask': asked[which], 'answer': correct, 'wrong': wrong})

    return out


def js_string(s):
    """A JavaScript string literal.

    MapleStory dialogue is full of backslash-r-backslash-n and #b#k colour
    codes, and the text itself contains quotes and apostrophes. Everything is
    escaped rather than cleverly quoted.
    """
    out = s.replace('\\', '\\\\').replace('"', '\\"')
    out = out.replace('\r', '\\r').replace('\n', '\\n')

    return '"' + out + '"'


FILE_HEADER = '''/* GENERATED from Quest.nx/Say.img by tools/gen_quest_scripts.py.
 *
 * Quest %(qid)s.%(kind)s Do not hand-edit: regenerate, or delete this file and
 * write a real one - the generator never overwrites a script that exists.
 *
 * The words are the game's own. What is NOT here is anything a conversation
 * cannot express: a quest that damages you, hands you an item mid-sentence or
 * branches on what you are carrying needs a hand-written script.
 *
 * EACH PHASE PICKS ITS OWN FORM. Handing a quest in can be a quiz while
 * starting it is a conversation, or the other way about - Say.img marks the
 * two phases independently and 350 quests ask their questions on the WAY IN.
 * Generating one of those as a conversation is what made Robin's quiz accept
 * every answer.
 */
'''


# A CONVERSATION: pages of text, then a question, then the reward.
CONVO = '''
var %(v)sStatus = -1;

var %(v)sSay = [%(say)s];%(extra)s

function %(fn)s(mode, type, selection) {
    if (mode == -1) {
        qm.dispose();
        return;
    }

%(backout)s
    if (mode == 1) {
        %(v)sStatus++;
    } else {
        %(v)sStatus--;
    }

    if (%(v)sStatus < 0) {
        qm.dispose();
        return;
    }

    if (%(v)sSay.length == 0) {
        qm.%(finish)s();
        qm.dispose();
        return;
    }

    if (%(v)sStatus < %(v)sSay.length - 1) {
        // The first message has no "previous" to go back to.
        if (%(v)sStatus == 0) {
            qm.sendNext(%(v)sSay[0]);
        } else {
            qm.sendNextPrev(%(v)sSay[%(v)sStatus]);
        }
    } else if (%(v)sStatus == %(v)sSay.length - 1) {
        // The last thing said before the question IS the question.
        qm.%(close)s(%(v)sSay[%(v)sStatus]);
    } else if (%(v)sStatus == %(v)sSay.length) {
        // The reward comes after the last word, not before it.
        qm.%(finish)s();
%(after)s
        qm.dispose();
    } else {
        qm.dispose();
    }
}
'''


# Backing out of the accept box. The decline line is worth saying - it is
# often the only personality a minor NPC gets.
CONVO_DECLINE = '''    if (mode == 0 && type > 0) {
        if (%(v)sStatus == %(v)sSay.length - 1 && %(v)sNo.length > 0) {
            qm.sendOk(%(v)sNo[0]);
        }

        qm.dispose();
        return;
    }

'''

CONVO_PLAIN_BACKOUT = '''    if (mode == 0 && type > 0) {
        qm.dispose();
        return;
    }

'''


# A QUIZ: Say.img marks the phase `ask = 1` and gives the correct option and a
# separate rebuke for every wrong one. Each question's text OPENS with the
# praise for the previous answer, which is why getting one right simply shows
# the next question.
QUIZ = '''
var %(v)sQ = 0;
var %(v)sPhase = "start";

var %(v)sQuestions = [%(questions)s];
var %(v)sAnswers = [%(answers)s];
var %(v)sWrong = [%(wrong)s];
var %(v)sClosing = %(closing)s;

function %(v)sAsk() {
    qm.sendSimple(%(v)sQuestions[%(v)sQ]);
}

function %(fn)s(mode, type, selection) {
    if (mode == -1) {
        qm.dispose();
        return;
    }

    // End Chat, or declining at the end. A selection arrives as type 4 and
    // must not be mistaken for one: mode is 0 there too on some clients.
    if (mode == 0 && type != 4) {
        qm.dispose();
        return;
    }

    if (%(v)sPhase == "start") {
        %(v)sPhase = "asking";
        %(v)sAsk();
        return;
    }

    // They have just read why the last answer was wrong. Ask again.
    if (%(v)sPhase == "rebuke") {
        %(v)sPhase = "asking";
        %(v)sAsk();
        return;
    }

    if (%(v)sPhase == "asking") {
        if (selection == %(v)sAnswers[%(v)sQ]) {
            %(v)sQ++;

            if (%(v)sQ < %(v)sQuestions.length) {
                %(v)sAsk();
                return;
            }

            %(v)sPhase = "closing";
            qm.%(close)s(%(v)sClosing);
            return;
        }

        %(v)sPhase = "rebuke";

        var why = %(v)sWrong[%(v)sQ]["" + selection];
        qm.sendOk(why ? why : "That is not the right answer. Have another go.");
        return;
    }

    // The last word has been read, so the reward is due.
    qm.%(finish)s();
%(after)s
    qm.dispose();
}
'''


def quiz_note(start_quiz, end_quiz):
    """The one-line "what is this" for the file header."""
    if start_quiz and end_quiz:
        return ' A QUIZ both on the way in and on the way out.'

    if start_quiz:
        return ' A QUIZ on the way in.'

    if end_quiz:
        return ' A QUIZ on the way out.'

    return ''


def phase_body(name, finish, node, say, questions, close, yes=None, no=None):
    """One phase - `start` or `end` - in whichever form Say.img asks for.

    `close` is how the LAST page is shown, and it is the only thing that
    really differs between the two phases: the way in ends with an accept
    box, the way out with an ordinary next. `finish` is what is called once
    that page is dismissed.
    """
    if questions:
        asked = [q['ask'] for q in questions]

        # The last thing in the phase that nobody asks about is the closing
        # line - "Bravo!" - shown after the final correct answer.
        closing = say[len(asked)] if len(say) > len(asked) else asked[-1]

        return QUIZ % {
            'v': name,
            'fn': name,
            'finish': finish,
            'close': close,
            'after': '',
            'questions': ', '.join(js_string(a) for a in asked),
            'answers': ', '.join(str(q['answer']) for q in questions),
            'wrong': ', '.join(
                '{' + ', '.join('"%d": %s' % (opt, js_string(text))
                                for opt, text in sorted(q['wrong'].items())) + '}'
                for q in questions),
            'closing': js_string(closing),
        }

    extra = ''
    after = ''
    backout = CONVO_PLAIN_BACKOUT

    if yes is not None:
        extra = ('\nvar %sYes = [%s];\nvar %sNo = [%s];'
                 % (name, ', '.join(js_string(s) for s in yes),
                    name, ', '.join(js_string(s) for s in no or [])))

        backout = CONVO_DECLINE % {'v': name}

        if yes:
            after = ('\n        if (%sYes.length > 0) {\n'
                     '            qm.sendOk(%sYes[0]);\n'
                     '        }\n' % (name, name))

    return CONVO % {
        'v': name,
        'fn': name,
        'finish': finish,
        'close': close,
        'say': ', '.join(js_string(s) for s in say),
        'extra': extra,
        'backout': backout,
        'after': after,
    }


def main():
    if len(sys.argv) < 3:
        raise SystemExit(__doc__)

    quest_nx, out_dir = sys.argv[1], sys.argv[2]
    force = '--force' in sys.argv

    nx = Nx(quest_nx)
    say = nx.resolve('Say.img')

    if say is None:
        raise SystemExit('no Say.img in ' + quest_nx)

    os.makedirs(out_dir, exist_ok=True)

    written = 0
    quizzes_start = 0
    quizzes_end = 0
    skipped_existing = 0
    skipped_empty = 0

    for qid, idx in nx.children(say):
        if not qid.isdigit():
            continue

        path = os.path.join(out_dir, qid + '.js')

        if os.path.exists(path) and not force:
            skipped_existing += 1
            continue

        start_node = nx.child(idx, '0')
        end_node = nx.child(idx, '1')

        start_say = lines(nx, start_node)
        end_say = lines(nx, end_node)

        # Nothing to say on either side - usually an auto-start quest whose
        # whole existence is a counter. A script that says nothing is worse
        # than no script, because it stops the shop opening.
        if not start_say and not end_say:
            skipped_empty += 1
            continue

        yes = lines(nx, nx.child(start_node, 'yes')) if start_node else []
        no = lines(nx, nx.child(start_node, 'no')) if start_node else []

        # EACH PHASE INDEPENDENTLY. A quiz is not a conversation and cannot be
        # generated as one - and Say.img marks the two phases separately, so a
        # quest may perfectly well ask its questions on the way IN and simply
        # talk on the way out.
        start_quiz = quiz(nx, start_node)
        end_quiz = quiz(nx, end_node)

        body = FILE_HEADER % {
            'qid': qid,
            'kind': quiz_note(start_quiz, end_quiz),
        }

        body += phase_body(
            'start', 'beginQuest', start_node, start_say, start_quiz,
            # Accepting is what STARTS a quest, so the last page of the way in
            # is an accept box however the phase got there.
            close='sendAcceptDecline', yes=yes, no=no)

        body += phase_body(
            'end', 'finishQuest', end_node, end_say, end_quiz,
            close='sendNext')

        if start_quiz:
            quizzes_start += 1

        if end_quiz:
            quizzes_end += 1

        with io.open(path, 'w', encoding='utf-8', newline='\n') as f:
            f.write(body)

        written += 1

    print('wrote %d scripts to %s' % (written, out_dir))
    print('  %d ask a QUIZ on the way IN, with real answers' % quizzes_start)
    print('  %d ask a QUIZ on the way OUT, with real answers' % quizzes_end)
    print('left %d hand-written scripts alone' % skipped_existing)
    print('skipped %d quests with no dialogue at all' % skipped_empty)


if __name__ == '__main__':
    main()
