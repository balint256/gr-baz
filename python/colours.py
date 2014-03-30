#!/usr/bin/env python

class Callable:
    def __init__(self, anycallable):
        self.__call__ = anycallable

class TerminalColours:
	Esc = '\033['
	EscFin = 'm'
	
	_Reset = 0
	_Bold = 1
	_Italic = 3
	_Underline = 4
	_DefaultBackground = 49
	Colours = range(8)
	(_Blk, _R, _G, _Y, _B, _M, _C, _W) = Colours
	
	Reset = Esc + str(_Reset) + EscFin
	Bold = Esc + str(_Bold) + EscFin
	Italic = Esc + str(_Italic) + EscFin
	Underline = Esc + str(_Underline) + EscFin
	DefaultBackground = Esc + str(_DefaultBackground) + EscFin
	
	def _lo(colour):
		return str(30 + colour)
	_lo = Callable(_lo)
	
	def _hi(colour):
		return str(90 + colour)
	_hi = Callable(_hi)
	
	def _bk(colour):
		return str(40 + colour)
	_bk = Callable(_bk)
	
	def _bkhi(colour):
		return str(100 + colour)
	_bkhi = Callable(_bkhi)
	
	def esc(colour):
		return TerminalColours.Esc + str(colour) + TerminalColours.EscFin
	esc = Callable(esc)
	
	def lo(colour):
		return TerminalColours.esc(TerminalColours._lo(colour))
	lo = Callable(lo)
	
	def hi(colour):
		return TerminalColours.esc(TerminalColours._hi(colour))
	hi = Callable(hi)
	
	def bk(colour):
		return TerminalColours.esc(TerminalColours._bk(colour))
	bk = Callable(bk)
	
	def bkhi(colour):
		return TerminalColours.esc(TerminalColours._bkhi(colour))
	bkhi = Callable(bkhi)
	
	#(Black, Red, Green, Yellow, Blue, Magenta, Cyan, White) = map(lo, Colours)
	#(Blk, R, G, Y, B, M, C, W) = map(TerminalColours.lo, Colours)
	#(BlackHi, RedHi, GreenHi, YellowHi, BlueHi, MagentaHi, CyanHi, WhiteHi)
	#(BlkHi, RHi, GHi, YHi, BHi, MHi, CHi, WHi) = map(hi, Colours)
	#(BackBlack, BackRed, BackGreen, BackYellow, BackBlue, BackMagenta, BackCyan, BackWhite)
	#(BkBlk, BkR, BkG, BkY, BkB, BkM, BkC, BkW) = map(bk, Colours)
	#(BackBlackHi, BackRedHi, BackGreenHi, BackYellowHi, BackBlueHi, BackMagentaHi, BackCyanHi, BackWhiteHi)
	#(BkBlkHi, BkRHi, BkGHi, BkYHi, BkBHi, BkMHi, BkCHi, BkWHi) = map(bkhi, Colours)
	
	def reset(str=""):
		return TerminalColours.Reset + str;
	reset = Callable(reset)
	
	def bold(str):
		return TerminalColours.Bold + str + TerminalColours.Reset;
	bold = Callable(bold)
	
	def italic(str):
		return TerminalColours.Italic + str + TerminalColours.Reset;
	italic = Callable(italic)

	def underline(str):
		return TerminalColours.Underline + str + TerminalColours.Reset;
	underline = Callable(underline)
	
	def colour(colour, msg):
		if isinstance(colour, int) or colour[0] != '\033':
			colour = TerminalColours.esc(colour)
		return colour + str(msg) + TerminalColours.Reset
	colour = Callable(colour)
	
	def pick_colour(colour, hi=False, bk=False):
		if hi:
			if bk:
				return TerminalColours.bkhi(colour)
			else:
				return TerminalColours.hi(colour)
		if bk:
			return TerminalColours.bk(colour)
		return TerminalColours.lo(colour)
	pick_colour = Callable(pick_colour)

	#def blk(str, hi, bk):
	#	return colour(pick_colour(_Blk, hi, bk), str)
	
	#(blk, r, g, y, b, m, c, w) = map(lambda x: (lambda y, z1=False, z2=False: colour(y, pick_colour(x, z1, z2))), Colours)
	
	def colours(str, colours, mapping):
		prev = None
		res = ""
		while i in range(len(str)):
			if prev is None or mapping[i] != prev:
				if mapping[i] < 0 or mapping[i] >= len(colours):
					colour = TerminalColours.Reset
				else:
					colour = colours[mapping[i]]
				str += colour
				if i < len(mapping):
					prev = mapping[i]
			res += str[i]
		return res + TerminalColours.Reset
	colours = Callable(colours)

tc = TerminalColours

(tc.Blk, tc.R, tc.G, tc.Y, tc.B, tc.M, tc.C, tc.W) = map(TerminalColours.lo, TerminalColours.Colours)
(tc.BlkHi, tc.RHi, tc.GHi, tc.YHi, tc.BHi, tc.MHi, tc.CHi, tc.WHi) = map(TerminalColours.hi, TerminalColours.Colours)
(tc.BkBlk, tc.BkR, tc.BkG, tc.BkY, tc.BkB, tc.BkM, tc.BkC, tc.BkW) = map(TerminalColours.bk, TerminalColours.Colours)
(tc.BkBlkHi, tc.BkRHi, tc.BkGHi, tc.BkYHi, tc.BkBHi, tc.BkMHi, tc.BkCHi, tc.BkWHi) = map(TerminalColours.bkhi, TerminalColours.Colours)
#(tc.blk, tc.r, tc.g, tc.y, tc.b, tc.m, tc.c, tc.w) = map(lambda (c): Callable(lambda (x): (lambda (y, z1, z2): TerminalColours.colour(y, TerminalColours.pick_colour(x, z1, z2)))), TerminalColours.Colours)
(tc.blk, tc.r, tc.g, tc.y, tc.b, tc.m, tc.c, tc.w) = map(lambda (c): Callable(c), map(lambda x: (lambda y, z1=None, z2=None: TerminalColours.colour(TerminalColours.pick_colour(x, z1, z2), y)), TerminalColours.Colours))
