#!/usr/bin/perl
# Makes the split screen build's three menu schemes from GoldenEye: Source's own
# resource/SourceScheme.res, with bigger fonts from 1080p up:
#
#   perl make_menu_schemes.pl <gesource>/resource/SourceScheme.res <out folder>
#
#   GESMenuScheme.res  the mod's menus laid out in pixels (Split Screen, Create Server)
#   GESPropScheme.res  the mod's proportional menus (credits, update checker, intro video)
#   SourceScheme.res   GameUI (main menu list, Options, server browser...)
#
# Sizes, as a multiple of the original 1080p size (yres 1024-1199):
#   1080p (yres 1024-1199)   SCALE_1080
#   1440p (yres 1200-1599)   SCALE_1080 x 1.5
#   4K    (yres 1600-10000)  SCALE_1080 x 2.5
# Below 1024 lines nothing changes. SCALE_1080 must match GE_MENU_SCALE_1080 in
# game/client/ges/vgui/ge_panelhelper.h (the panels' layout uses the same steps).
#
# Proportional panels already grow with the screen height (h/480), so their fonts
# get SCALE_1080 x 1 / 1.125 / 1.25 (1440p: 1.125 x 1440/1080 = 1.5; 4K: 1.25 x 2 = 2.5).
#
# GameUI's own dialogs (Options, server browser, achievements...) are closed
# source with fixed pixel layouts (rows 16-24 pixels): in SourceScheme.res only
# MenuLarge (the main menu list, also copied as MainMenuFont) gets the full sizes;
# the other fonts grow to at most GAMEUI_MAX_TALL so their text isn't cut off.
use strict; use warnings;

my $SCALE_1080 = 1.5;
my $GAMEUI_MAX_TALL = 16;
my $MAXTALL = 127;    # the biggest glyphs vgui's font cache takes

my @ranges = ( '1024 1199', '1200 1599', '1600 10000' );
my @menuF = map { $_ * $SCALE_1080 } ( 1, 1.5, 2.5 );
my @propF = map { $_ * $SCALE_1080 } ( 1, 1.125, 1.25 );
my %fullInGameUI = map { $_ => 1 } qw(MenuLarge MainMenuFont);

my ( $in, $outDir ) = @ARGV;
die "usage: make_menu_schemes.pl SourceScheme.res outfolder\n" unless $in && $outDir;
open my $fh, '<:raw', $in or die "$in: $!";
my $txt = do { local $/; <$fh> }; close $fh;
my $nl = $txt =~ /\r\n/ ? "\r\n" : "\n";
my @lines = split /\r?\n/, $txt, -1;

Write( "GESMenuScheme.res", 'menu',
	"Split screen build: SourceScheme.res for the mod's menus laid out in pixels (Split Screen, Create Server).",
	"Fonts: 1080p x$SCALE_1080, 1440p x" . $menuF[1] . ", 4K x" . $menuF[2] . " their original 1080p size (made by splitscreen_resources/tools/make_menu_schemes.pl)." );
Write( "GESPropScheme.res", 'prop',
	"Split screen build: SourceScheme.res for the mod's proportional menus (credits, update checker, intro video).",
	"Proportional fonts already grow with the screen (h/480): 1080p x$SCALE_1080, 1440p x" . $propF[1] . ", 4K x" . $propF[2] . " (made by make_menu_schemes.pl)." );
Write( "SourceScheme.res", 'gameui',
	"Split screen build: MenuLarge / MainMenuFont (the main menu list) 1080p x$SCALE_1080, 1440p x" . $menuF[1] . ", 4K x" . $menuF[2] . ";",
	"the other fonts grow to at most $GAMEUI_MAX_TALL pixels, GameUI's dialogs being fixed size (made by make_menu_schemes.pl)." );

sub Write {
	my ( $file, $mode, @header ) = @_;
	my @out = ( ( map { "// $_" } @header ), Fonts( $mode ) );
	open my $oh, '>:raw', "$outDir/$file" or die "$outDir/$file: $!";
	print $oh join( $nl, @out );
	close $oh;
	print STDERR "wrote $outDir/$file\n";
}

sub strip { my $l = shift; $l =~ s{//.*$}{}; $l =~ s/^\s+|\s+$//g; return $l; }

sub Fonts {
	my ($mode) = @_;
	my $fi = -1;
	for my $i ( 0 .. $#lines ) { if ( strip( $lines[$i] ) =~ /^"?Fonts"?$/ ) { $fi = $i; last; } }
	die "no Fonts section" if $fi < 0;
	my $i = $fi + 1;
	$i++ while strip( $lines[$i] ) ne '{';
	my @out = @lines[ 0 .. $i ];
	$i++;
	my $hasMainMenuFont = grep { strip($_) =~ /^"?MainMenuFont"?/ } @lines;
	while (1) {
		my $s = strip( $lines[$i] );
		last if $s eq '}';
		if ( $s eq '' ) { push @out, $lines[$i++]; next; }
		my $nameLine = $lines[$i];
		my ($name) = $s =~ /^"?([^"\s\[]+)"?/;
		my $j = $i + 1;
		$j++ while strip( $lines[$j] ) eq '';
		die "expected { after $name" if strip( $lines[$j] ) ne '{';
		my ( $d, $k, @blk ) = ( 0, $j );
		for ( ; ; $k++ ) {
			my $t = strip( $lines[$k] );
			$d++ if $t eq '{'; $d-- if $t eq '}';
			push @blk, $lines[$k];
			last if $d == 0;
		}
		$i = $k + 1;
		push @out, Transform( $nameLine, \@blk, $mode, $name );
		if ( $mode eq 'gameui' && $name eq 'MenuLarge' && !$hasMainMenuFont ) {
			# GameUI's main menu list asks for MainMenuFont first, then MenuLarge
			( my $copy = $nameLine ) =~ s/"?MenuLarge"?/"MainMenuFont"/;
			push @out, '', "\t\t// The main menu list (GameUI tries MainMenuFont before MenuLarge)", Transform( $copy, \@blk, $mode, 'MainMenuFont' );
		}
	}
	push @out, @lines[ $i .. $#lines ];
	return @out;
}

sub NewTall {
	my ( $t, $r, $mode, $name ) = @_;
	my $f = ( $mode eq 'prop' ? \@propF : \@menuF )->[$r];
	my $n = int( $t * $f + 0.5 );
	if ( $mode eq 'gameui' && !$fullInGameUI{$name} ) {
		$n = $t >= $GAMEUI_MAX_TALL ? $t : ( $n > $GAMEUI_MAX_TALL ? $GAMEUI_MAX_TALL : $n );
	}
	return $n > $MAXTALL ? $MAXTALL : $n;
}

# Blocks reaching 1024+ lines stop at 1023 (no yres: "1 1023"); blocks only for
# 1024+ go; then copies of the blocks used at 1080 for each new range.
sub Transform {
	my ( $nameLine, $blk, $mode, $name ) = @_;
	my @b = @$blk;
	my @res = ( $nameLine, $b[0] );
	my ( @subs, $p );
	for ( $p = 1 ; $p < $#b ; ) {
		my $s = strip( $b[$p] );
		if ( $s ne '' && $s ne '{' && $s ne '}' && $p + 1 < $#b && strip( $b[ $p + 1 ] ) eq '{' ) {
			my ( @sb, $d, $q ) = ( $b[$p] );
			for ( $d = 0, $q = $p + 1 ; ; $q++ ) {
				my $t = strip( $b[$q] );
				$d++ if $t eq '{'; $d-- if $t eq '}';
				push @sb, $b[$q];
				last if $d == 0;
			}
			push @subs, [ 'sub', \@sb ];
			$p = $q + 1;
		}
		else { push @subs, [ 'raw', $b[ $p++ ] ]; }
	}
	my @at1080;
	for my $e (@subs) {
		if ( $e->[0] eq 'raw' ) { push @res, $e->[1]; next; }
		my @sb = @{ $e->[1] };
		my ( $min, $max ) = ( 0, 0 );
		for (@sb) { ( $min, $max ) = ( $1, $2 eq '' ? $1 : $2 ) if /"yres"\s*"\s*(\d+)\s*(\d*)\s*"/; }
		my $ranged = $min > 0;    # vgui ignores a yres starting at 0
		push @at1080, [@sb] if !$ranged || ( $min <= 1080 && 1080 <= $max );
		if ( $ranged && $max < 1024 ) { push @res, @sb; next; }
		next if $ranged && $min >= 1024;
		push @res, SetYres( \@sb, ( $ranged ? $min : 1 ) . " 1023" );
	}
	for my $r ( 0 .. $#ranges ) {
		my $n = 0;
		for my $sb (@at1080) {
			my @c = @$sb;
			( my $tag = $ranges[$r] ) =~ s/ /-/;
			$n++;
			$c[0] =~ s/"?([^"\s\[]+)"?/"yres$tag\_$n"/;
			s/("tall"\s*")(\d+)(")/$1 . NewTall( $2, $r, $mode, $name ) . $3/e for @c;
			push @res, SetYres( \@c, $ranges[$r] );
		}
	}
	push @res, $b[-1];
	return @res;
}

sub SetYres {
	my ( $sb, $range ) = @_;
	my @c = @$sb;
	return @c if grep { s/("yres"\s*")[^"]*(")/$1$range$2/ } @c;
	my $keyInd = "\t\t\t\t";
	for ( 2 .. $#c - 1 ) { if ( $c[$_] =~ /^(\s*)"/ ) { $keyInd = $1; last; } }
	splice @c, $#c, 0, "$keyInd\"yres\"\t\"$range\"";
	return @c;
}
