#!/usr/bin/perl
# Builds index.html from slides.md: one "## " section per slide; Визуал picks the image, Бележки become speaker notes.
use strict; use warnings; use utf8; binmode STDOUT, ':utf8';
open my $f, '<:utf8', 'slides.md' or die; local $/; my $md = <$f>; close $f;
my @secs = split /^## /m, $md; shift @secs;
my %have = map { $_ => 1 } map { s{^img/}{}r } glob('img/*.png');
sub inl { my $t = shift; $t =~ s/&/&amp;/g; $t =~ s/</&lt;/g; $t =~ s/\*\*(.+?)\*\*/<b>$1<\/b>/g; $t =~ s/`([^`]+)`/<code>$1<\/code>/g; return $t; }
my $out = ''; my $n = 0;
for my $s (@secs) {
  $s =~ s/\n---\s*$//s; my ($title, $body) = split /\n/, $s, 2; $body //= '';
  $title =~ s/^\d+\.\s*//; $n++;
  my ($vis) = $body =~ /\*\*Визуал:\*\*(.*?)(?:\n\n|\z)/s; $vis //= '';
  my ($notes) = $body =~ /\*\*Бележки:\*\*(.*?)(?:\n\n|\z)/s; $notes //= '';
  my @imgs = grep { $have{$_} } ($vis =~ /`(?:[^`]*\/)?([\w.-]+\.png)`/g);
  $body =~ s/\*\*Визуал:\*\*.*?(?:\n\n|\z)//s; $body =~ s/\*\*Бележки:\*\*.*?(?:\n\n|\z)//s;
  my $html = ''; my $inlist = 0; my $intable = 0;
  for my $l (split /\n/, $body) {
    if ($l =~ /^\s*[-*]\s+(.*)/) { $html .= "<ul>" unless $inlist; $inlist = 1; $html .= "<li>" . inl($1) . "</li>"; next; }
    $html .= "</ul>" if $inlist; $inlist = 0;
    if ($l =~ /^\|(.*)\|\s*$/) { next if $l =~ /^\|[\s:|-]+\|\s*$/; my @c = map { inl($_ =~ s/^\s+|\s+$//gr) } split /\|/, $1; $html .= "<table>" unless $intable; my $tag = $intable ? 'td' : 'th'; $intable = 1; $html .= "<tr>" . join('', map { "<$tag>$_</$tag>" } @c) . "</tr>"; next; }
    $html .= "</table>" if $intable; $intable = 0;
    next if $l =~ /^\s*$/ || $l =~ /^>/;
    $html .= "<p>" . inl($l) . "</p>";
  }
  $html .= "</ul>" if $inlist; $html .= "</table>" if $intable;
  my $img = @imgs ? join('', map { "<figure><img src=\"img/$_\" alt=\"\"></figure>" } @imgs[0 .. ($#imgs > 1 ? 1 : $#imgs)]) : '';
  my $cls = $n == 1 ? 'slide title' : ($img ? 'slide split' . (@imgs > 1 ? ' two' : '') : 'slide text');
  $out .= "<section class=\"$cls\" data-n=\"$n\"><div class=\"copy\"><div class=\"kicker\">" . sprintf('%02d', $n) . " · Енергийна криза</div><h2>" . inl($title) . "</h2>$html</div>" . ($img ? "<div class=\"media\">$img</div>" : '') . "<aside class=\"notes\">" . inl($notes =~ s/^\s+|\s+$//gr) . "</aside></section>\n";
}
open my $t, '<:utf8', 'template.html' or die; my $tpl = <$t>; close $t;
$tpl =~ s/__SLIDES__/$out/; open my $o, '>:utf8', 'index.html'; print $o $tpl; close $o; print "slides=$n\n";
