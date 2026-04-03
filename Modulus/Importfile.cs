using System;
using Autodesk.AutoCAD.DatabaseServices;

namespace ImportFile
{
    public class FileService
    {
        public static Database GetFile(string filePath)
        {
            Database db = new Database(false, true);
            try
            {
                db.ReadDwgFile(filePath, System.IO.FileShare.Read, true, "");
                return db;
            }
            catch (System.Exception ex)
            {
                db.Dispose();
                throw new Exception("Could not load the DWG: " + ex.Message);
            }
        }
    }
}