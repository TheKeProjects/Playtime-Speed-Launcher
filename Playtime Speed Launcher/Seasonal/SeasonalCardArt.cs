using System.Windows;
using System.Windows.Media;

namespace SpeedrunLauncher.Seasonal;

/// <summary>
/// Decorations for the tall chapter cards. Everything stays in the top part of the card
/// (or sits on its top edge, sticking out up to ~56 px above it) so the title, playtime
/// and action buttons at the bottom are never covered.
/// </summary>
public static class SeasonalCardArt
{
    private static SolidColorBrush Frozen(byte r, byte g, byte b, byte a = 255)
    {
        var brush = new SolidColorBrush(Color.FromArgb(a, r, g, b));
        brush.Freeze();
        return brush;
    }

    private static readonly Brush OrnRed    = Frozen(211, 47, 47);
    private static readonly Brush OrnGold   = Frozen(255, 202, 40);
    private static readonly Brush OrnBlue   = Frozen(66, 165, 245);
    private static readonly Brush OrnPurple = Frozen(171, 71, 188);
    private static readonly Brush SnowCap   = Frozen(240, 247, 255);
    private static readonly Pen   SnowEdge  = MakePen(Frozen(170, 200, 230), 1.0);

    private static Pen MakePen(Brush brush, double thickness)
    {
        var pen = new Pen(brush, thickness) { LineJoin = PenLineJoin.Round };
        pen.Freeze();
        return pen;
    }

    public static void Draw(string season, DrawingContext dc, Size size, int variant)
    {
        double w = size.Width, h = size.Height;
        if (w < 120 || h < 160) return;
        var s = Math.Clamp(w / 150.0, 1.0, 2.8);
        switch (season)
        {
            case "halloween": Halloween(dc, w, s, variant); break;
            case "summer":    Summer(dc, w, s, variant);    break;
            case "christmas": Christmas(dc, w, s, variant); break;
        }
    }

    private static void Halloween(DrawingContext dc, double w, double s, int variant)
    {
        switch (variant % 6)
        {
            case 0:
                HalloweenArt.DrawCobweb(dc, 0, 0, s * 1.1, false, false);
                HalloweenArt.DrawSpider(dc, w - 34 * s, 0, 52 * s, s * 0.95);
                break;
            case 1:
                HalloweenArt.DrawWitchHat(dc, w - 22 * s, 3, s * 0.9, 14);
                HalloweenArt.DrawBat(dc, 30 * s, 34 * s, s);
                HalloweenArt.DrawBat(dc, 54 * s, 22 * s, s * 0.7);
                break;
            case 2:
                HalloweenArt.DrawCobweb(dc, w, 0, s * 1.1, true, false);
                HalloweenArt.DrawSpider(dc, 26 * s, 0, 30 * s, s * 0.9);
                HalloweenArt.DrawSpider(dc, 50 * s, 0, 62 * s, s * 0.75);
                break;
            case 3:
                HalloweenArt.DrawPumpkin(dc, 28 * s, -8 * s, s * 1.15);
                HalloweenArt.DrawPumpkin(dc, 50 * s, -6 * s, s * 0.8);
                HalloweenArt.DrawGhost(dc, w - 26 * s, 32 * s, s * 1.1);
                break;
            case 4:
                HalloweenArt.DrawCobweb(dc, 0, 0, s * 1.0, false, false);
                HalloweenArt.DrawWitchHat(dc, w - 22 * s, 3, s * 0.9, 14);
                break;
            default:
                HalloweenArt.DrawCobweb(dc, w, 0, s * 0.9, true, false);
                HalloweenArt.DrawBat(dc, 28 * s, 30 * s, s);
                HalloweenArt.DrawPumpkin(dc, w * 0.5, -8 * s, s * 1.05);
                break;
        }
    }

    private static void Summer(DrawingContext dc, double w, double s, int variant)
    {
        switch (variant % 6)
        {
            case 0:
                SummerArt.DrawSun(dc, 6 * s, 6 * s, s * 1.1);
                SummerArt.DrawSeagull(dc, w - 34 * s, 22 * s, s * 1.1);
                SummerArt.DrawSeagull(dc, w - 56 * s, 34 * s, s * 0.8);
                break;
            case 1:
                SummerArt.DrawBeachBall(dc, w - 30 * s, -9 * s, s * 1.15);
                SummerArt.DrawStarfish(dc, 22 * s, 22 * s, s * 1.2, 15);
                break;
            case 2:
                SummerArt.DrawUmbrella(dc, 30 * s, 2, s * 0.7, -8);
                SummerArt.DrawShell(dc, w - 22 * s, 22 * s, s * 1.2, -20);
                break;
            case 3:
                SummerArt.DrawCrab(dc, w - 34 * s, -2 * s, s * 1.15);
                SummerArt.DrawSeagull(dc, 30 * s, 26 * s, s * 1.1);
                break;
            case 4:
                SummerArt.DrawIceCream(dc, w - 24 * s, 26 * s, s * 1.15);
                SummerArt.DrawStarfish(dc, 22 * s, 22 * s, s * 1.1, -10);
                SummerArt.DrawShell(dc, w * 0.5, -5 * s, s, 0);
                break;
            default:
                SummerArt.DrawSun(dc, w - 6 * s, 6 * s, s * 1.1);
                SummerArt.DrawCrab(dc, 34 * s, -2 * s, s * 1.05);
                break;
        }
    }

    private static void Christmas(DrawingContext dc, double w, double s, int variant)
    {
        DrawSnowCap(dc, w, s, variant);
        switch (variant % 6)
        {
            case 0:
                ChristmasArt.DrawSantaHat(dc, 20 * s, 0, s * 1.1, -16);
                ChristmasArt.DrawHolly(dc, w - 20 * s, 22 * s, s * 1.2, 0);
                break;
            case 1:
                ChristmasArt.DrawOrnament(dc, w - 26 * s, 6 * s, 40 * s, s, OrnRed);
                ChristmasArt.DrawOrnament(dc, w - 52 * s, 6 * s, 18 * s, s * 0.8, OrnGold);
                ChristmasArt.DrawOrnament(dc, 30 * s, 6 * s, 28 * s, s * 0.9, OrnBlue);
                break;
            case 2:
                ChristmasArt.DrawCandyCane(dc, w - 24 * s, 34 * s, s, 15);
                ChristmasArt.DrawSnowflake(dc, 26 * s, 30 * s, s * 1.2);
                break;
            case 3:
                ChristmasArt.DrawSantaHat(dc, w - 20 * s, 0, s * 1.1, 16);
                ChristmasArt.DrawGingerbread(dc, 24 * s, 34 * s, s * 1.2, -8);
                break;
            case 4:
                ChristmasArt.DrawHolly(dc, 20 * s, 20 * s, s * 1.2, 180);
                ChristmasArt.DrawOrnament(dc, w - 30 * s, 6 * s, 30 * s, s, OrnPurple);
                ChristmasArt.DrawOrnament(dc, w * 0.5, 6 * s, 14 * s, s * 0.8, OrnGold);
                break;
            default:
                ChristmasArt.DrawGift(dc, 26 * s, 1, s * 1.1, OrnRed);
                ChristmasArt.DrawGift(dc, 48 * s, 1, s * 0.8, OrnBlue);
                ChristmasArt.DrawSnowman(dc, w - 26 * s, 2, s * 0.8);
                break;
        }
    }

    /// <summary>Wavy snow drift resting on the card's top edge, with a few icicle-like drips.</summary>
    private static void DrawSnowCap(DrawingContext dc, double w, double s, int variant)
    {
        var geo = new StreamGeometry();
        using (var ctx = geo.Open())
        {
            var x0 = 2.0; var x1 = w - 2;
            ctx.BeginFigure(new Point(x0, 2), true, true);
            // Lumpy top.
            const int bumps = 7;
            for (int i = 0; i < bumps; i++)
            {
                var a = x0 + (x1 - x0) * i / bumps;
                var b = x0 + (x1 - x0) * (i + 1) / bumps;
                ctx.BezierTo(new Point(a + (b - a) * 0.2, -5 * s), new Point(a + (b - a) * 0.8, -5 * s), new Point(b, -1 * s), true, true);
            }
            ctx.LineTo(new Point(x1, 4 * s), true, true);
            // Bottom edge with drips.
            var rnd = new Random(variant * 31 + 7);
            var x = x1;
            while (x > x0 + 8 * s)
            {
                var step = (8 + rnd.NextDouble() * 10) * s;
                var nx = Math.Max(x0, x - step);
                var depth = rnd.Next(3) == 0 ? (6 + rnd.NextDouble() * 6) * s : (3 + rnd.NextDouble() * 2) * s;
                ctx.BezierTo(new Point(x - step * 0.2, depth), new Point(nx + step * 0.2, depth), new Point(nx, 4 * s), true, true);
                x = nx;
            }
            ctx.LineTo(new Point(x0, 4 * s), true, true);
        }
        geo.Freeze();
        dc.DrawGeometry(SnowCap, SnowEdge, geo);
    }
}
