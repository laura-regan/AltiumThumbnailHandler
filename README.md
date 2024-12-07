# AltiumThumbnailHandler

Thumbnail handler for Altium Designer .SchLib and .PcbLib file types. The handler renders the first symbol/footprint in the library file


To register the handler:
```
regsvr32 AltiumThumbnailProviderCpp.dll
```

To unregister the handler:

```
regsvr32 /u AltiumThumbnailProviderCpp.dll
```

![Symbol Thumbnail Handler](./Images/symbol_thumbnail.png)
![Footprint Thumbnail Handler](./Images/footprint_thumbnail.png)



