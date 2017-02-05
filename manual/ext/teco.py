from pygments.lexer import RegexLexer
from pygments.token import *

class TecoLexer(RegexLexer):
	name = 'Teco'
	aliases = ['teco']
	filenames = ['*.teco']

	string = r'"(\\"|[^"])*"'
	char = r'[a-zA-Z.0-9_]+'

	tokens = {
		'root': [

			(r'\s+', Text),
			(r'/\*', Comment.Multiline, 'comment'),
			(r'//.*?\n', Comment.Single),

			(r'steol.*?$', String),
			(string, String),

			(r'#' + char, Keyword),
			(r':?@' + char, Name.Label),

			(r'\$' + char, Name.Variable),

			# Types
			(r'(int|uint|float|str|label)\b', Keyword.Type),

			# Keyword
			(r'(macro|return|macroname)\b', Keyword),
			(r'(if|else|for|while|do|continue|break|FOR|WHILE|DO|goto|enum)\b', Keyword),
			(r'(Seg|SegEnd|Raw|Com|Error|Warning)\b', Keyword),

			(r'.', Text),
		],
		'comment': [
			(r'[^*/]', Comment.Multiline),
			(r'\*/', Comment.Multiline, '#pop'),
			(r'[*/]', Comment.Multiline)
		]

#			(r' .*\n', Text),
#			(r'\+.*\n', Generic.Inserted),
#			(r'-.*\n', Generic.Deleted),
#			(r'@.*\n', Generic.Subheading),
#			(r'Index.*\n', Generic.Heading),
#			(r'=.*\n', Generic.Heading),
#			(r'.*\n', Text),
	}

def setup(app):
	app.add_lexer('teco', TecoLexer())

