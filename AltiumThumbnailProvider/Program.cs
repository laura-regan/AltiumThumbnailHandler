
using OriginalCircuit.AltiumSharp.Drawing;
using OriginalCircuit.AltiumSharp;
using OpenMcdf;
using System.Drawing;
using System.IO;
using System.Net.NetworkInformation;

namespace AltiumThumbnailProvider
{
    internal static class Program
    {

        public static void Main(string[] args)
        {
            if (args != null)
            { 
                if (args.Length == 2)
                {
                    string filePath = args[0];
                    int cx = Convert.ToInt32(args[1], 10);
                    var thumbnail = new Bitmap(cx, cx);

                    CompoundFile cf = new CompoundFile(filePath);
                    CFStream foundStream = cf.RootStorage.GetStream("FileHeader");
                    byte[] temp = foundStream.GetData();
                    cf.Close();

                    String fileHeader = System.Text.Encoding.Default.GetString(temp);

                    System.Diagnostics.Debug.WriteLine(fileHeader);

                    if (fileHeader.Contains("PCB 6.0 Binary Library File"))
                    {
                        using (var reader = new PcbLibReader())
                        {
                            PcbLib lib = reader.Read(filePath);

                            using (var renderer = new PcbLibRenderer())
                            {
                                // Render first footprint in library
                                renderer.Component = lib.Items[0];
                                renderer.BackgroundColor = Color.Black;

                                using (var g = Graphics.FromImage(thumbnail))
                                {
                                    renderer.Render(g, cx, cx, true, false);
                                }
                            }
                        }
                    }
                    else if (fileHeader.Contains("Schematic Library"))
                    {
                        using (var reader = new SchLibReader())
                        {
                            SchLib lib = reader.Read(filePath);

                            using (var renderer = new SchLibRenderer(lib.Header, lib))
                            {
                                // Render first symbol in library
                                renderer.Component = lib.Items[0];
                                renderer.BackgroundColor = Color.White;
                                renderer.ScreenDpi = 100;
                                
                                using (var g = Graphics.FromImage(thumbnail))
                                {
                                    g.InterpolationMode = System.Drawing.Drawing2D.InterpolationMode.NearestNeighbor;
                                    g.CompositingQuality = System.Drawing.Drawing2D.CompositingQuality.HighQuality;
                                    g.SmoothingMode = System.Drawing.Drawing2D.SmoothingMode.None;
                                    renderer.Render(g, cx, cx, true, false);
                                }
                            }
                        }
                    }

                    if (thumbnail != null)
                    {
                        filePath = Path.ChangeExtension(filePath, ".bmp");
                        thumbnail.Save(filePath, System.Drawing.Imaging.ImageFormat.Bmp);
                    }
                }
            }
        }
    }
}
